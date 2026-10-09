// ko12.8: mons.pak (pak_core.cpp): AES, clave, CTR por posicion y tabla de nombres
#include "framework.h"
#include "../pak_core.h"
#include <string.h>
#include <vector>
#include <string>

static std::string hex(const uint8_t *p, size_t n) {
  static const char *H = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) { s += H[p[i] >> 4]; s += H[p[i] & 15]; }
  return s;
}
static std::vector<uint8_t> unhex(const char *s) {
  std::vector<uint8_t> v;
  for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
    auto nib = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    v.push_back((uint8_t)(nib(s[i]) << 4 | nib(s[i + 1])));
  }
  return v;
}

TEST(pak, aes_fips197) {
  auto k = unhex("000102030405060708090a0b0c0d0e0f"), p = unhex("00112233445566778899aabbccddeeff");
  PakAes a;
  a.setKey(k.data());
  uint8_t out[16];
  a.block(p.data(), out);
  CHECK(hex(out, 16) == "69c4e0d86a7b0430d8cdb78070b4c55a");
}

TEST(pak, sha256_abc) {
  uint8_t d[32];
  pakSha256((const uint8_t *)"abc", 3, d);
  CHECK(hex(d, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  std::string big(1000, 'a');  // varios bloques
  pakSha256((const uint8_t *)big.data(), big.size(), d);
  CHECK(hex(d, 32) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

// mismos valores que da tools/make_pak.py (comprobado contra la libreria "cryptography")
TEST(pak, igual_que_la_herramienta_del_pc) {
  uint8_t key[16];
  pakDeriveKey(PAK_DEFAULT_PASS, key);
  CHECK(hex(key, 16) == "469b58fd6a8191c8a614705d85f4a373");
  PakAes a;
  a.setKey(key);
  uint8_t chk[16];
  pakMakeCheck(a, chk);
  CHECK(hex(chk, 16) == "7d0d8ecee12ade675dea0788fb1ab0f8");
  const uint8_t salt[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  uint8_t d[40];
  for (int i = 0; i < 40; i++) d[i] = (uint8_t)i;
  pakCrypt(a, salt, 5, d, 40);
  CHECK(hex(d, 40) == "5b4b55e42c68d681d18a282854be9938ab8dccc13d6e97b008941cef278adbebc8d9d4c76f23f6a8");
  // por trozos sueltos sale lo mismo (leer a mitad de fichero)
  uint8_t e[40];
  for (int i = 0; i < 40; i++) e[i] = (uint8_t)i;
  pakCrypt(a, salt, 5, e, 7);
  pakCrypt(a, salt, 12, e + 7, 20);
  pakCrypt(a, salt, 32, e + 27, 13);
  CHECK(memcmp(d, e, 40) == 0);
  pakCrypt(a, salt, 5, e, 40);  // otra vez = descifrar
  for (int i = 0; i < 40; i++) CHECK_EQ((int)e[i], i);
}

// un .pak entero en memoria: cabecera, datos, tabla; y leerlo de vuelta
TEST(pak, montar_y_leer) {
  struct F { const char *name; std::string data; };
  std::vector<F> files = { { "bgm.wav", std::string(1000, 'm') }, { "fx/f0100.bin", "fx!" }, { "p001.bin", "PMD-sprite" }, { "thumbs.bin", "TPTH...." } };
  uint8_t key[16];
  pakDeriveKey("otra frase", key);
  PakAes a;
  a.setKey(key);
  PakHdr h;
  for (int i = 0; i < 8; i++) h.salt[i] = (uint8_t)(0xA0 + i);
  std::vector<uint8_t> pak(PAK_HDR, 0);
  std::vector<uint8_t> tbl;
  for (auto &f : files) {
    uint32_t off = (uint32_t)pak.size(), sz = (uint32_t)f.data.size();
    std::vector<uint8_t> enc(f.data.begin(), f.data.end());
    pakCrypt(a, h.salt, off, enc.data(), enc.size());
    pak.insert(pak.end(), enc.begin(), enc.end());
    uint8_t rec[9] = { (uint8_t)off, (uint8_t)(off >> 8), (uint8_t)(off >> 16), (uint8_t)(off >> 24),
                       (uint8_t)sz, (uint8_t)(sz >> 8), (uint8_t)(sz >> 16), (uint8_t)(sz >> 24), (uint8_t)strlen(f.name) };
    tbl.insert(tbl.end(), rec, rec + 9);
    tbl.insert(tbl.end(), f.name, f.name + strlen(f.name));
  }
  h.count = (uint32_t)files.size();
  h.indexOff = (uint32_t)pak.size();
  h.indexSize = (uint32_t)tbl.size();
  std::vector<uint8_t> etbl = tbl;
  pakCrypt(a, h.salt, h.indexOff, etbl.data(), etbl.size());
  pak.insert(pak.end(), etbl.begin(), etbl.end());
  pakMakeCheck(a, h.check);
  pakWriteHeader(h, pak.data());

  // leer
  PakHdr r;
  CHECK(pakParseHeader(pak.data(), r));
  CHECK_EQ(r.count, (uint32_t)4);
  PakAes ok;
  ok.setKey(key);
  CHECK(pakKeyOk(ok, r));
  uint8_t bad[16];
  pakDeriveKey(PAK_DEFAULT_PASS, bad);
  PakAes wrong;
  wrong.setKey(bad);
  CHECK(!pakKeyOk(wrong, r));  // otra frase: no se acepta
  std::vector<uint8_t> t2(pak.begin() + r.indexOff, pak.begin() + r.indexOff + r.indexSize);
  pakCrypt(ok, r.salt, r.indexOff, t2.data(), t2.size());
  PakIndex idx;
  CHECK(idx.parse(t2.data(), (uint32_t)t2.size(), r.count, (uint32_t)pak.size()));
  CHECK(idx.hasPrefix("fx/"));
  CHECK(!idx.hasPrefix("story/"));
  CHECK(idx.find("nada.bin") == nullptr);
  for (auto &f : files) {
    const PakEntry *e = idx.find(f.name);
    CHECK(e != nullptr);
    if (!e) continue;
    CHECK_EQ(e->size, (uint32_t)f.data.size());
    std::vector<uint8_t> got(pak.begin() + e->off, pak.begin() + e->off + e->size);
    pakCrypt(ok, r.salt, e->off, got.data(), got.size());
    CHECK(std::string(got.begin(), got.end()) == f.data);
  }
  // una tabla rota (desordenada / fuera del fichero) no se acepta
  PakIndex bad2;
  CHECK(!bad2.parse(t2.data(), (uint32_t)t2.size(), r.count, 100));
  std::vector<uint8_t> magic(pak.begin(), pak.begin() + PAK_HDR);
  magic[0] = 'X';
  CHECK(!pakParseHeader(magic.data(), r));
}

// ---- ko12.8.3: mapa de sectores (cadena FAT -> tramos) con un disco simulado ----
namespace {
struct SimDisk {
  std::vector<uint8_t> d;  // sectores de 512
  int reads = 0;
  explicit SimDisk(uint32_t sectors) : d((size_t)sectors * 512, 0) {}
  static bool rd(uint32_t s, uint32_t c, uint8_t *buf, void *ctx) {
    SimDisk *k = (SimDisk *)ctx;
    k->reads++;
    if ((uint64_t)(s + c) * 512 > k->d.size()) return false;
    memcpy(buf, k->d.data() + (size_t)s * 512, (size_t)c * 512);
    return true;
  }
  void setFat(const PakFatGeo &g, uint32_t c, uint32_t v) {
    uint8_t *p = d.data() + (size_t)g.fatbase * 512 + (size_t)c * (g.fat32 ? 4 : 2);
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    if (g.fat32) { p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
  }
};
// escribe un fichero de 'size' bytes en los clusteres 'cl' (en ese orden) y devuelve su contenido
std::vector<uint8_t> simFile(SimDisk &k, const PakFatGeo &g, const std::vector<uint32_t> &cl, uint32_t size, uint32_t seed) {
  std::vector<uint8_t> v(size);
  for (uint32_t i = 0; i < size; i++) { seed = seed * 1103515245u + 12345u; v[i] = (uint8_t)(seed >> 16); }
  uint32_t cb = g.csize * 512;
  for (size_t i = 0; i < cl.size(); i++) {
    uint32_t sect = g.database + (cl[i] - 2) * g.csize;
    uint32_t from = (uint32_t)i * cb, n = from < size ? (size - from < cb ? size - from : cb) : 0;
    if (n) memcpy(k.d.data() + (size_t)sect * 512, v.data() + from, n);
    k.setFat(g, cl[i], i + 1 < cl.size() ? cl[i + 1] : (g.fat32 ? 0x0FFFFFFFu : 0xFFFFu));
  }
  return v;
}
}  // namespace

TEST(pak, mapa_fat32_fragmentado) {
  PakFatGeo g{ true, 32, 32 + 64, 4, 3000 };  // FAT de 64 sectores, clusteres de 2 KB
  SimDisk k(32 + 64 + 3000 * 4);
  // 3 trozos seguidos, uno suelto hacia atras y otro largo: 23 clusteres
  std::vector<uint32_t> cl;
  for (uint32_t c = 10; c < 15; c++) cl.push_back(c);
  for (uint32_t c = 200; c < 207; c++) cl.push_back(c);
  cl.push_back(7);
  for (uint32_t c = 2900; c < 2910; c++) cl.push_back(c);
  uint32_t size = 22 * 2048 + 777;  // el ultimo cluster a medias
  auto ref = simFile(k, g, cl, size, 99);
  uint8_t win[4096], bounce[4096];
  PakExt ex[16];
  int n = pakChainExtents(g, cl[0], size, SimDisk::rd, &k, win, ex, 16);
  CHECK(n == 4);
  CHECK(ex[0].off == 0 && ex[0].nsect == 20 && ex[1].off == 5 * 2048 && ex[3].nsect == 40);
  // lecturas por todas partes (cruzando tramos, a mitad de sector, hasta el final)
  uint32_t seed = 7;
  for (int t = 0; t < 400; t++) {
    seed = seed * 1664525u + 1013904223u;
    uint32_t off = seed % size;
    uint32_t len = 1 + (seed >> 8) % 9000;
    if (off + len > size) len = size - off;
    std::vector<uint8_t> got(len);
    uint32_t r = pakExtRead(ex, n, off, got.data(), len, SimDisk::rd, &k, bounce);
    CHECK(r == len);
    CHECK(memcmp(got.data(), ref.data() + off, len) == 0);
  }
  CHECK(pakExtFind(ex, n, 23 * 2048) == nullptr);  // fuera del ultimo cluster
}

TEST(pak, mapa_fat16_y_contiguo) {
  PakFatGeo g{ false, 4, 4 + 16, 8, 2000 };
  SimDisk k(4 + 16 + 2000 * 8);
  std::vector<uint32_t> cl;
  for (uint32_t c = 50; c < 150; c++) cl.push_back(c);  // todo seguido: un solo tramo
  uint32_t size = 100 * 4096;
  auto ref = simFile(k, g, cl, size, 3);
  uint8_t win[4096], bounce[4096];
  PakExt ex[4];
  int n = pakChainExtents(g, 50, size, SimDisk::rd, &k, win, ex, 4);
  CHECK(n == 1 && ex[0].nsect == 800);
  std::vector<uint8_t> got(size);
  CHECK(pakExtRead(ex, n, 0, got.data(), size, SimDisk::rd, &k, bounce) == size);
  CHECK(got == ref);
}

TEST(pak, mapa_cadena_rota) {
  PakFatGeo g{ true, 32, 32 + 64, 4, 3000 };
  SimDisk k(32 + 64 + 3000 * 4);
  std::vector<uint32_t> cl = { 10, 11, 12 };
  simFile(k, g, cl, 3 * 2048, 1);
  uint8_t win[4096];
  PakExt ex[8];
  CHECK(pakChainExtents(g, 10, 3 * 2048, SimDisk::rd, &k, win, ex, 8) == 1);
  CHECK(pakChainExtents(g, 10, 4 * 2048, SimDisk::rd, &k, win, ex, 8) == -1);  // la cadena es mas corta
  k.setFat(g, 11, 5000);  // apunta fuera del volumen
  CHECK(pakChainExtents(g, 10, 3 * 2048, SimDisk::rd, &k, win, ex, 8) == -1);
  k.setFat(g, 11, 0);     // cluster libre en medio
  CHECK(pakChainExtents(g, 10, 3 * 2048, SimDisk::rd, &k, win, ex, 8) == -1);
  CHECK(pakChainExtents(g, 1, 100, SimDisk::rd, &k, win, ex, 8) == -1);  // inicio invalido
  CHECK(pakChainExtents(g, 10, 0, SimDisk::rd, &k, win, ex, 8) == 0);   // vacio
  // mas trozos que sitio para tramos
  std::vector<uint32_t> sp = { 100, 300, 500, 700 };
  simFile(k, g, sp, 4 * 2048, 2);
  CHECK(pakChainExtents(g, 100, 4 * 2048, SimDisk::rd, &k, win, ex, 3) == -1);
  CHECK(pakChainExtents(g, 100, 4 * 2048, SimDisk::rd, &k, win, ex, 4) == 4);
}

TEST(pak, mapa_lee_poco_la_fat) {
  // 270 MB con clusteres de 4 KB = 69k clusteres: la FAT se lee por ventanas de 8 sectores
  PakFatGeo g{ true, 32, 32 + 600, 8, 70000 };
  uint32_t nclu = 69000;
  SimDisk k(32 + 600 + 8);  // solo la FAT (los datos no se tocan al hacer el mapa)
  for (uint32_t c = 2; c < 2 + nclu; c++) k.setFat(g, c, c + 1 < 2 + nclu ? c + 1 : 0x0FFFFFFFu);
  uint8_t win[4096];
  PakExt ex[4];
  int n = pakChainExtents(g, 2, nclu * 4096u, SimDisk::rd, &k, win, ex, 4);
  CHECK(n == 1 && ex[0].nsect == nclu * 8);
  CHECK(k.reads <= (int)((nclu * 4 / 512) / 8 + 2));  // ~68 lecturas de 4 KB, no una por cluster
}
