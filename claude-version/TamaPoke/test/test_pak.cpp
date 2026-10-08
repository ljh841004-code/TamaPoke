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
