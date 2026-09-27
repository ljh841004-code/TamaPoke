// ko11.6: copia de la partida en la SD (formato y comprobaciones)
#include "framework.h"
#include "../savebak.h"
#include <vector>
#include <string>

struct Seen { std::vector<std::string> keys; size_t bytes = 0; };
static bool collect(void *ctx, const char *ns, const char *key, uint8_t, const uint8_t *, size_t len) {
  Seen *s = (Seen *)ctx;
  s->keys.push_back(std::string(ns) + "/" + key);
  s->bytes += len;
  return true;
}

TEST(savebak, ida_y_vuelta) {
  std::vector<uint8_t> buf(4096);
  BakWriter w(buf.data(), buf.size());
  w.begin(7, 1790000000u, 25, 31);
  uint8_t full = 80;
  CHECK(w.add("tamapoke", "full", 1, &full, 1));
  uint8_t mons[36] = { 1, 2, 3 };
  CHECK(w.add("tphall", "mons", 0x42, mons, sizeof(mons)));
  CHECK(w.add("tamapoke", "nick", 0x21, "pika", 5));
  size_t n = w.finish();
  CHECK(n > sizeof(BakHdr));
  BakHdr h;
  Seen s;
  CHECK(bakParse(buf.data(), n, &h, collect, &s));
  CHECK_EQ(h.seq, 7u);
  CHECK_EQ(h.dex, (int16_t)25);
  CHECK_EQ(h.lvl, (uint16_t)31);
  CHECK_EQ(h.count, (uint16_t)3);
  CHECK_EQ(s.keys.size(), (size_t)3);
  CHECK(s.keys[1] == "tphall/mons");
  CHECK_EQ(s.bytes, (size_t)(1 + 36 + 5));
}

TEST(savebak, fichero_danado_o_cortado_no_vale) {
  std::vector<uint8_t> buf(1024);
  BakWriter w(buf.data(), buf.size());
  w.begin(1, 0, 4, 5);
  uint16_t v = 1234;
  w.add("tamapoke", "exp", 2, &v, 2);
  size_t n = w.finish();
  BakHdr h;
  CHECK(bakParse(buf.data(), n, &h));
  for (size_t i = 0; i < n; i++) {  // un bit cambiado en cualquier sitio
    buf[i] ^= 0x10;
    CHECK(!bakParse(buf.data(), n, &h));
    buf[i] ^= 0x10;
  }
  for (size_t cut = 0; cut < n; cut++) CHECK(!bakParse(buf.data(), cut, &h));  // cortado
  CHECK(!bakParse(nullptr, 100, &h));
}

TEST(savebak, no_cabe_no_se_escribe) {
  std::vector<uint8_t> buf(64);
  BakWriter w(buf.data(), buf.size());
  w.begin(1, 0, 1, 1);
  uint8_t big[100] = {};
  CHECK(!w.add("tamapoke", "dexreg", 0x42, big, sizeof(big)));
  CHECK_EQ(w.finish(), (size_t)0);
  BakWriter w2(buf.data(), buf.size());
  w2.begin(1, 0, 1, 1);
  CHECK(!w2.add("un_espacio_muy_largo", "k", 1, big, 1));  // nombres de NVS: 15 max
}

// ko11.6.1: la marca de copia manual va en la cabecera
TEST(savebak, marca_manual) {
  std::vector<uint8_t> buf(256);
  BakWriter w(buf.data(), buf.size());
  w.begin(3, 0, 25, 9, BAKF_MANUAL);
  size_t n = w.finish();
  BakHdr h;
  CHECK(bakParse(buf.data(), n, &h));
  CHECK(h.flags & BAKF_MANUAL);
  BakWriter a(buf.data(), buf.size());
  a.begin(4, 0, 25, 9);
  n = a.finish();
  CHECK(bakParse(buf.data(), n, &h));
  CHECK_EQ(h.flags, 0u);
}
