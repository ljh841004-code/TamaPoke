#include "framework.h"
#include "../wav_stream.h"
#include "../music_route.h"
#include "../bgm_pick.h"
#include <memory>
#include <algorithm>

struct MemoryFile {
  std::shared_ptr<std::vector<uint8_t>> bytes;
  uint32_t at = 0;
  size_t maxRead = 8192;
  MemoryFile() = default;
  explicit MemoryFile(std::vector<uint8_t> b) : bytes(std::make_shared<std::vector<uint8_t>>(std::move(b))) {}
  explicit operator bool() const { return bool(bytes); }
  size_t size() const { return bytes ? bytes->size() : 0; }
  bool seek(uint32_t p) { at = p; return p <= size(); }
  size_t read(uint8_t *p, size_t n) {
    n = std::min(n, std::min(maxRead, size() - at));
    memcpy(p, bytes->data() + at, n); at += n; return n;
  }
  void close() { bytes.reset(); }
};
static void put32(std::vector<uint8_t>& b, size_t p, uint32_t n) {
  for (int i=0;i<4;i++) b[p+i] = n >> (8*i);
}
static std::vector<uint8_t> wav(uint32_t samples, bool metadata = false) {
  std::vector<uint8_t> b = {'R','I','F','F',0,0,0,0,'W','A','V','E',
    'f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0x3e,0,0,0,0x7d,0,0,2,0,16,0};
  if (metadata) {
    const uint8_t junk[] = {'J','U','N','K',3,0,0,0,1,2,3,0};
    b.insert(b.end(),junk,junk+12);
  }
  size_t data=b.size(); b.resize(data+8+samples*2);
  memcpy(b.data()+data,"data",4);put32(b,data+4,samples*2);
  for(uint32_t i=0;i<samples;i++) { b[data+8+2*i]=i&255;b[data+9+2*i]=(i>>8)&255; }
  put32(b,4,b.size()-8); return b;
}
TEST(Audio, Full170SecondSongAndLoop) {
  const uint32_t length = 16000*170+13;
  WavStream<MemoryFile> stream;
  CHECK(stream.open(MemoryFile(wav(length,true))));
  int16_t block[256]; bool matched=true;
  for(uint32_t start=0;start<length+512;start+=256) {
    if(stream.read(block,256)!=256) {matched=false;break;}
    for(uint32_t i=0;i<256;i++) if(block[i]!=(int16_t)((start+i)%length)) matched=false;
  }
  CHECK(matched);
  CHECK(sizeof(stream)<9000);
}
TEST(Audio, SmallDataChunkLoopsWithinBlock) {
  WavStream<MemoryFile> s;CHECK(s.open(MemoryFile(wav(3))));
  int16_t b[256];CHECK_EQ(s.read(b,256),256u);
  for(int i=0;i<256;i++) CHECK_EQ(b[i],i%3);
}
TEST(Audio, ResumeUsesConsumedPositionNotReadAhead) {
  WavStream<MemoryFile> s;CHECK(s.open(MemoryFile(wav(10000))));
  int16_t b[256];s.read(b,256);CHECK_EQ(s.position(),512u);
  auto cursor=s.position();s.close();CHECK(s.open(MemoryFile(wav(10000)),cursor));
  s.read(b,256);CHECK_EQ(b[0],256);
}
TEST(Audio, ReadAheadWithoutCardLockDoesNotTouchFile) {
  WavStream<MemoryFile> s;CHECK(s.open(MemoryFile(wav(10000))));
  int16_t b[256];CHECK_EQ(s.read(b,256,false),0u);
  CHECK_EQ(s.read(b,256),256u);
  for(int i=0;i<15;i++) CHECK_EQ(s.read(b,256,false),256u);
  CHECK_EQ(s.read(b,256,false),0u);CHECK_EQ(s.position(),8192u);
  CHECK_EQ(s.read(b,256),256u);CHECK_EQ(b[0],4096);
}
TEST(Audio, RejectTruncatedOversizedAndOddChunks) {
  WavStream<MemoryFile> s;
  auto b=wav(10);b.pop_back();CHECK(!s.open(MemoryFile(b)));
  b=wav(10);put32(b,40,0xffffffff);CHECK(!s.open(MemoryFile(b)));
  b=wav(10);put32(b,40,19);CHECK(!s.open(MemoryFile(b)));
  b=wav(0);CHECK(!s.open(MemoryFile(b)));
  b=wav(10);put32(b,4,0xffffffff);CHECK(!s.open(MemoryFile(b)));
}
TEST(Audio, RejectUnsupportedFormats) {
  WavStream<MemoryFile> s;
  for(int field : {20,22,24,28,32,34}) {
    auto b=wav(10);b[field]^=1;CHECK(!s.open(MemoryFile(b)));
  }
}
TEST(Audio, IoFailureClosesAndDoesNotLoop) {
  WavStream<MemoryFile> s;MemoryFile f(wav(10000));f.maxRead=100;
  CHECK(s.open(f));int16_t b[256];CHECK_EQ(s.read(b,256),0u);CHECK(!s.valid());
}
TEST(Audio, BattleRoutingAllOutcomesAndTrainer) {
  CHECK(wildMusicActive(true,false,0,false));
  for(int outcome=1;outcome<=4;outcome++) CHECK(!wildMusicActive(true,false,outcome,false));
  CHECK(!wildMusicActive(false,false,0,false));
  CHECK(!wildMusicActive(true,true,0,false));
  CHECK(!wildMusicActive(true,false,0,true));
}
TEST(Audio, WaveResultsResumeButNewRunRestarts) {
  MusicRoute r;
  CHECK_EQ(r.switchTo(0,1,0,false),0u); // boot BGM
  CHECK_EQ(r.switchTo(3,1,512,true),0u); // first wild in session 1
  CHECK(!r.changed(3,1)); // repeated loop/wave request never reopens
  CHECK_EQ(r.switchTo(2,1,123456,true),0u); // victory -> normal from start
  CHECK_EQ(r.switchTo(3,1,2048,true),123456u); // next wild resumes
  CHECK_EQ(r.switchTo(2,1,234568,true),0u); // trainer / exit / flee
  CHECK_EQ(r.switchTo(3,1,4096,true),234568u);
  CHECK_EQ(r.switchTo(5,1,88888,true),0u); // NEW run must reset
  CHECK(r.changed(5,2)); // explicit reload after replacement
}

// ko11: el contador de vueltas sube al terminar la cancion (para sortear la siguiente)
TEST(Audio, WavStreamCountsLoops) {
  WavStream<MemoryFile> ws;
  CHECK(ws.open(MemoryFile(wav(3))));
  CHECK_EQ(ws.loops, 0u);
  int16_t buf[3];
  CHECK_EQ(ws.read(buf, 3), (size_t)3);
  CHECK_EQ(ws.loops, 0u);
  CHECK_EQ(ws.read(buf, 1), (size_t)1);  // vuelve a empezar
  CHECK_EQ(ws.loops, 1u);
}

// ko11.2: el anillo se rellena a trozos en cuanto hay hueco (margen siempre casi lleno)
TEST(Audio, ReadAheadTopsUpInChunks) {
  WavStream<MemoryFile> s;CHECK(s.open(MemoryFile(wav(100000))));
  int16_t b[256];
  for(int i=0;i<4;i++) CHECK_EQ(s.read(b,256),256u);  // 2 KiB consumidos -> se rellena
  int n=0;while(s.read(b,256,false)==256u) n++;
  CHECK_EQ(n,16);  // 8 KiB enteros de colchon, no lo que sobraba del bloque anterior
  CHECK_EQ(b[0],(int16_t)(4*256+15*256));
}

// ko11.8: fondos elegibles
TEST(Bgm, ChooseOnlyEnabledAndNeverRepeatsWhenThereIsAnother) {
  // hay 0,1,2; activadas 0 y 2
  for (uint32_t r = 0; r < 50; r++) {
    uint8_t n = bgmChoose(0x07, 0x05, 0, r);
    CHECK_EQ(n, (uint8_t)2);           // no repite la 0, y la 1 esta apagada
    CHECK_EQ(bgmChoose(0x07, 0x05, 2, r), (uint8_t)0);
  }
  CHECK_EQ(bgmChoose(0x07, 0x02, 1, 7), (uint8_t)1);   // una sola activada: se repite
  CHECK_EQ(bgmChoose(0x01, 0x02, 0, 3), (uint8_t)0);   // la activada no existe: la que hay
  CHECK_EQ(bgmChoose(0x00, 0xFF, 0, 3), (uint8_t)0);   // nada en la SD: bgm.wav
  bool seen[3] = {false, false, false};
  for (uint32_t r = 0; r < 30; r++) seen[bgmChoose(0x07, 0xFF, 8, r)] = true;  // al empezar, cualquiera
  CHECK(seen[0] && seen[1] && seen[2]);
}
TEST(Bgm, WavInfoReadsLengthAndTitle) {
  std::vector<uint8_t> b = wav(16000 * 3);
  // anadir LIST/INFO/INAM "Pallet Town" al final
  const char *t = "Pallet Town";
  std::vector<uint8_t> inam = {'I','N','A','M',12,0,0,0};
  inam.insert(inam.end(), t, t + 11); inam.push_back(0);
  std::vector<uint8_t> list = {'L','I','S','T',0,0,0,0,'I','N','F','O'};
  list.insert(list.end(), inam.begin(), inam.end());
  put32(list, 4, list.size() - 8);
  b.insert(b.end(), list.begin(), list.end());
  put32(b, 4, b.size() - 8);
  MemoryFile f(b);
  uint32_t bytes = 0; char title[28];
  CHECK(wavInfo(f, &bytes, title, sizeof(title)));
  CHECK_EQ(bytes, (uint32_t)(16000 * 3 * 2));
  CHECK(strcmp(title, "Pallet Town") == 0);
  MemoryFile g(wav(100));
  CHECK(wavInfo(g, &bytes, title, sizeof(title)));
  CHECK_EQ(title[0], 0);
  std::vector<uint8_t> bad = wav(100); bad[0] = 'X';
  MemoryFile h(bad);
  CHECK(!wavInfo(h, &bytes, title, sizeof(title)));
}
TEST(Bgm, SlotFromRenamedFileNames) {  // ko11.8.1
  bool ex;
  CHECK_EQ(bgmSlotFromName("bgm.wav", &ex), 0); CHECK(ex);
  CHECK_EQ(bgmSlotFromName("bgm2.wav", &ex), 1); CHECK(ex);
  CHECK_EQ(bgmSlotFromName("bgm8.wav", &ex), 7); CHECK(ex);
  CHECK_EQ(bgmSlotFromName("bgm2 (1).wav", &ex), 1); CHECK(!ex);
  CHECK_EQ(bgmSlotFromName("BGM2.WAV", &ex), 1);
  CHECK_EQ(bgmSlotFromName("bgm2-1.wav"), 1);
  CHECK_EQ(bgmSlotFromName("bgm (2).wav", &ex), 0); CHECK(!ex);
  CHECK_EQ(bgmSlotFromName("bgm1.wav", &ex), 0); CHECK(!ex);
  CHECK_EQ(bgmSlotFromName("bgm10.wav"), -1);
  CHECK_EQ(bgmSlotFromName("bgm9.wav"), -1);
  CHECK_EQ(bgmSlotFromName("bgm0.wav"), -1);
  CHECK_EQ(bgmSlotFromName("._bgm2.wav"), -1);
  CHECK_EQ(bgmSlotFromName("bgm2.mp3"), -1);
  CHECK_EQ(bgmSlotFromName("battle_wild.wav"), -1);
  CHECK_EQ(bgmSlotFromName("b"), -1);
  CHECK_EQ(bgmSlotFromName(""), -1);
}
