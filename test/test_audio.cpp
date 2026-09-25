#include "framework.h"
#include "../wav_stream.h"
#include "../music_route.h"
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
