#pragma once
// ko11.6: copia de seguridad de la partida en la SD.
// La partida sigue guardandose en la NVS (rapido y seguro); esto solo hace una
// COPIA en la SD de vez en cuando, para poder recuperarla si la NVS se borra
// (instalar la imagen completa en 0x0, placa nueva, etc.).
//
// Formato (little endian):
//   cabecera BakHdr (24 bytes)
//   registros: u8 lenNs, ns, u8 lenKey, key, u8 tipo (nvs_type_t), u16 len, datos
//   u32 CRC32 de todo lo anterior
// Sin la WiFi ("tpnet"): la SD se puede sacar y leer en cualquier PC.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define BAK_VER 1
#define BAK_MAX_BYTES (64u * 1024u)

struct __attribute__((packed)) BakHdr {
  char magic[4];     // "TPBK"
  uint16_t ver;
  uint16_t count;    // registros
  uint32_t seq;      // cual es la mas nueva de las dos ranuras
  uint32_t epoch;    // cuando se hizo (0 = sin reloj)
  int16_t dex;       // el que se estaba criando (para ensenarlo al restaurar)
  uint16_t lvl;
  uint32_t reserved;
};
static_assert(sizeof(BakHdr) == 24, "BakHdr");

static inline uint32_t bakCrc32(const uint8_t *p, size_t n, uint32_t crc = 0) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

class BakWriter {
public:
  BakWriter(uint8_t *buf, size_t cap) : b_(buf), cap_(cap) {}
  void begin(uint32_t seq, uint32_t epoch, int16_t dex, uint16_t lvl) {
    n_ = 0;
    ok_ = cap_ >= sizeof(BakHdr) + 4;
    if (!ok_) return;
    BakHdr h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, "TPBK", 4);
    h.ver = BAK_VER;
    h.seq = seq;
    h.epoch = epoch;
    h.dex = dex;
    h.lvl = lvl;
    memcpy(b_, &h, sizeof(h));
    n_ = sizeof(h);
  }
  // false si no cabe (la copia entera se descarta)
  bool add(const char *ns, const char *key, uint8_t type, const void *data, size_t len) {
    size_t ln = strlen(ns), lk = strlen(key);
    if (!ok_ || ln > 15 || lk > 15 || len > 0xFFFF) return ok_ = false;
    size_t need = 1 + ln + 1 + lk + 1 + 2 + len;
    if (n_ + need + 4 > cap_) return ok_ = false;
    b_[n_++] = (uint8_t)ln; memcpy(b_ + n_, ns, ln); n_ += ln;
    b_[n_++] = (uint8_t)lk; memcpy(b_ + n_, key, lk); n_ += lk;
    b_[n_++] = type;
    b_[n_++] = (uint8_t)(len & 0xFF); b_[n_++] = (uint8_t)(len >> 8);
    if (len) memcpy(b_ + n_, data, len);
    n_ += len;
    BakHdr *h = (BakHdr *)b_;
    h->count++;
    return true;
  }
  // tamano final (con el CRC), 0 si algo no cupo
  size_t finish() {
    if (!ok_) return 0;
    uint32_t c = bakCrc32(b_, n_);
    memcpy(b_ + n_, &c, 4);
    return n_ + 4;
  }
private:
  uint8_t *b_;
  size_t cap_, n_ = 0;
  bool ok_ = false;
};

// Comprueba cabecera, tamanos y CRC. Con fn != nullptr recorre los registros
// (fn devuelve false para abortar). Nunca lee fuera de buf.
typedef bool (*BakRecFn)(void *ctx, const char *ns, const char *key, uint8_t type, const uint8_t *data,
                         size_t len);
static inline bool bakParse(const uint8_t *buf, size_t len, BakHdr *hdr, BakRecFn fn = nullptr,
                            void *ctx = nullptr) {
  if (!buf || len < sizeof(BakHdr) + 4) return false;
  BakHdr h;
  memcpy(&h, buf, sizeof(h));
  if (memcmp(h.magic, "TPBK", 4) || h.ver != BAK_VER) return false;
  uint32_t c;
  memcpy(&c, buf + len - 4, 4);
  if (bakCrc32(buf, len - 4) != c) return false;
  size_t p = sizeof(BakHdr), end = len - 4;
  for (uint16_t i = 0; i < h.count; i++) {
    char ns[16], key[16];
    if (p + 1 > end) return false;
    size_t ln = buf[p++];
    if (ln > 15 || p + ln + 1 > end) return false;
    memcpy(ns, buf + p, ln); ns[ln] = 0; p += ln;
    size_t lk = buf[p++];
    if (lk > 15 || p + lk + 3 > end) return false;
    memcpy(key, buf + p, lk); key[lk] = 0; p += lk;
    uint8_t type = buf[p++];
    size_t dl = buf[p] | (size_t)buf[p + 1] << 8;
    p += 2;
    if (p + dl > end) return false;
    if (fn && !fn(ctx, ns, key, type, buf + p, dl)) return false;
    p += dl;
  }
  if (p != end) return false;
  if (hdr) *hdr = h;
  return true;
}

// ---- en la placa (savebak_sd.cpp) ----
struct BakSlot { bool ok; BakHdr h; };
void bakInfo(BakSlot out[2]);                         // las dos ranuras de la SD (CRC comprobado)
int bakNewest(const BakSlot s[2]);                    // -1 si no hay ninguna valida
bool bakBackupNow(int16_t dex, uint16_t lvl, uint32_t epoch);  // escribe en la ranura mas vieja
bool bakRestore(uint8_t slot);                        // NVS <- ranura; hay que reiniciar despues
