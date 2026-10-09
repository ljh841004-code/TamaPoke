#include "pak_core.h"
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- AES-128 (FIPS-197)
static const uint8_t SBOX[256] = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

void PakAes::setKey(const uint8_t key[16]) {
  static const uint8_t RCON[10] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };
  memcpy(rk, key, 16);
  for (int i = 16, r = 0; i < 176; i += 4) {
    uint8_t t[4] = { rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1] };
    if (i % 16 == 0) {
      uint8_t a = t[0];
      t[0] = SBOX[t[1]] ^ RCON[r++];
      t[1] = SBOX[t[2]];
      t[2] = SBOX[t[3]];
      t[3] = SBOX[a];
    }
    for (int k = 0; k < 4; k++) rk[i + k] = rk[i - 16 + k] ^ t[k];
  }
}

static inline uint8_t xt(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }

void PakAes::block(const uint8_t in[16], uint8_t out[16]) const {
  uint8_t s[16];
  for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
  for (int r = 1; r <= 10; r++) {
    uint8_t t[16];
    for (int i = 0; i < 16; i++) t[i] = SBOX[s[i]];
    // ShiftRows (columnas de 4 bytes: estado en orden de columnas)
    uint8_t u[16];
    for (int c = 0; c < 4; c++)
      for (int row = 0; row < 4; row++) u[c * 4 + row] = t[((c + row) % 4) * 4 + row];
    if (r < 10) {
      for (int c = 0; c < 4; c++) {  // MixColumns
        uint8_t *a = u + c * 4;
        uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], all = a0 ^ a1 ^ a2 ^ a3;
        a[0] ^= all ^ xt(a0 ^ a1);
        a[1] ^= all ^ xt(a1 ^ a2);
        a[2] ^= all ^ xt(a2 ^ a3);
        a[3] ^= all ^ xt(a3 ^ a0);
      }
    }
    for (int i = 0; i < 16; i++) s[i] = u[i] ^ rk[r * 16 + i];
  }
  memcpy(out, s, 16);
}

// ---------------------------------------------------------------- SHA-256
static inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void pakSha256(const uint8_t *data, size_t n, uint8_t out[32]) {
  static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  };
  uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
  uint64_t bits = (uint64_t)n * 8;
  size_t total = ((n + 9 + 63) / 64) * 64;
  for (size_t off = 0; off < total; off += 64) {
    uint8_t blk[64];
    for (int i = 0; i < 64; i++) {
      size_t p = off + i;
      uint8_t b = 0;
      if (p < n) b = data[p];
      else if (p == n) b = 0x80;
      else if (p >= total - 8) b = (uint8_t)(bits >> (8 * (total - 1 - p)));
      blk[i] = b;
    }
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)blk[i * 4] << 24 | (uint32_t)blk[i * 4 + 1] << 16 | (uint32_t)blk[i * 4 + 2] << 8 | blk[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
      uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  for (int i = 0; i < 8; i++)
    for (int k = 0; k < 4; k++) out[i * 4 + k] = (uint8_t)(h[i] >> (24 - 8 * k));
}

void pakDeriveKey(const char *pass, uint8_t key[16]) {
  static const char PRE[] = "TamaPoke-pak:";
  size_t lp = strlen(PRE), ln = pass ? strlen(pass) : 0;
  uint8_t buf[160];
  if (ln > sizeof(buf) - lp) ln = sizeof(buf) - lp;
  memcpy(buf, PRE, lp);
  if (ln) memcpy(buf + lp, pass, ln);
  uint8_t d[32];
  pakSha256(buf, lp + ln, d);
  memcpy(key, d, 16);
}

// ---------------------------------------------------------------- cabecera
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

bool pakParseHeader(const uint8_t h[PAK_HDR], PakHdr &out) {
  if (memcmp(h, PAK_MAGIC, 4) != 0) return false;
  if ((h[4] | h[5] << 8) != PAK_VERSION) return false;
  out.count = rd32(h + 8);
  out.indexOff = rd32(h + 12);
  out.indexSize = rd32(h + 16);
  memcpy(out.salt, h + 20, 8);
  memcpy(out.check, h + 28, 16);
  out.dataOff = rd32(h + 44);
  return out.dataOff >= PAK_HDR && out.indexOff >= out.dataOff && out.count < 100000;
}

void pakWriteHeader(const PakHdr &h, uint8_t out[PAK_HDR]) {
  memset(out, 0, PAK_HDR);
  memcpy(out, PAK_MAGIC, 4);
  out[4] = PAK_VERSION;
  wr32(out + 8, h.count);
  wr32(out + 12, h.indexOff);
  wr32(out + 16, h.indexSize);
  memcpy(out + 20, h.salt, 8);
  memcpy(out + 28, h.check, 16);
  wr32(out + 44, h.dataOff);
}

void pakMakeCheck(const PakAes &aes, uint8_t out[16]) { aes.block((const uint8_t *)PAK_CHECK_TEXT, out); }

bool pakKeyOk(const PakAes &aes, const PakHdr &h) {
  uint8_t c[16];
  pakMakeCheck(aes, c);
  return memcmp(c, h.check, 16) == 0;
}

void pakCrypt(const PakAes &aes, const uint8_t salt[8], uint32_t abs, uint8_t *buf, size_t n) {
  uint8_t ctr[16], ks[16];
  memcpy(ctr, salt, 8);
  uint64_t blk = abs / 16;
  size_t skip = abs % 16;
  while (n) {
    for (int i = 0; i < 8; i++) ctr[8 + i] = (uint8_t)(blk >> (56 - 8 * i));
    aes.block(ctr, ks);
    for (size_t i = skip; i < 16 && n; i++, n--) *buf++ ^= ks[i];
    skip = 0;
    blk++;
  }
}

// ---------------------------------------------------------------- tabla
void PakIndex::clear() {
  free(e);
  e = nullptr;
  n = 0;
}

static int nameCmp(const char *a, uint8_t la, const char *b, uint8_t lb) {
  int c = memcmp(a, b, la < lb ? la : lb);
  return c ? c : (int)la - (int)lb;
}

bool PakIndex::parse(const uint8_t *tbl, uint32_t tblSize, uint32_t count, uint32_t fileSize) {
  clear();
  if (!count) return true;
  e = (PakEntry *)malloc(sizeof(PakEntry) * count);
  if (!e) return false;
  uint32_t p = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (p + 9 > tblSize) { clear(); return false; }
    PakEntry &x = e[i];
    x.off = rd32(tbl + p);
    x.size = rd32(tbl + p + 4);
    x.len = tbl[p + 8];
    p += 9;
    if (!x.len || x.len > PAK_NAME_MAX || p + x.len > tblSize) { clear(); return false; }
    x.name = (const char *)tbl + p;
    p += x.len;
    if (x.off < PAK_HDR || (uint64_t)x.off + x.size > fileSize) { clear(); return false; }
    if (i && nameCmp(e[i - 1].name, e[i - 1].len, x.name, x.len) >= 0) { clear(); return false; }  // ordenada y sin repetidos
  }
  n = count;
  return true;
}

const PakEntry *PakIndex::find(const char *rel) const {
  size_t l = strlen(rel);
  if (!l || l > PAK_NAME_MAX) return nullptr;
  uint32_t lo = 0, hi = n;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    int c = nameCmp(e[mid].name, e[mid].len, rel, (uint8_t)l);
    if (!c) return &e[mid];
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}

bool PakIndex::hasPrefix(const char *prefix) const {
  size_t l = strlen(prefix);
  for (uint32_t i = 0; i < n; i++)
    if (e[i].len > l && memcmp(e[i].name, prefix, l) == 0) return true;
  return false;
}

// ---------------------------------------------------------------- ko12.8.3: mapa de sectores
int pakChainExtents(const PakFatGeo &g, uint32_t sclust, uint32_t size, PakSectorRead rd, void *ctx, uint8_t *win,
                    PakExt *out, int maxOut) {
  if (!g.csize || !rd || !win || !out || maxOut < 1) return -1;
  const uint32_t cbytes = g.csize * 512u;
  const uint32_t need = size ? (uint32_t)(((uint64_t)size + cbytes - 1) / cbytes) : 0;
  if (!need) return 0;
  if (sclust < 2 || sclust >= g.nFatent) return -1;
  const uint32_t esz = g.fat32 ? 4 : 2, eoc = g.fat32 ? 0x0FFFFFF8u : 0xFFF8u;
  uint32_t winSect = 0xFFFFFFFFu;  // primer sector cargado en win (8 seguidos)
  int n = 0;
  uint32_t c = sclust;
  for (uint32_t k = 0; k < need; k++) {
    if (c < 2 || c >= g.nFatent) return -1;  // rota (o mas corta que el fichero)
    uint32_t sect = g.database + (c - 2) * g.csize;
    if (n && out[n - 1].sect + out[n - 1].nsect == sect) {
      out[n - 1].nsect += g.csize;
    } else {
      if (n >= maxOut) return -1;
      out[n].off = k * cbytes;
      out[n].sect = sect;
      out[n].nsect = g.csize;
      n++;
    }
    if (k + 1 == need) break;  // el ultimo: no hace falta leer su siguiente
    uint32_t byteOff = c * esz, fs = g.fatbase + byteOff / 512;
    if (fs < winSect || fs >= winSect + 8) {
      if (!rd(fs, 8, win, ctx)) return -1;
      winSect = fs;
    }
    const uint8_t *p = win + (fs - winSect) * 512 + byteOff % 512;
    uint32_t nx = g.fat32 ? ((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24) & 0x0FFFFFFFu
                          : (uint32_t)p[0] | (uint32_t)p[1] << 8;
    if (nx >= eoc) return -1;  // se acaba antes que el fichero
    c = nx;
  }
  return n;
}

const PakExt *pakExtFind(const PakExt *e, int n, uint32_t off) {
  int lo = 0, hi = n;  // ultimo tramo con e.off <= off
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (e[mid].off <= off) lo = mid + 1; else hi = mid;
  }
  if (lo == 0) return nullptr;
  const PakExt *x = &e[lo - 1];
  return (uint64_t)off < (uint64_t)x->off + (uint64_t)x->nsect * 512u ? x : nullptr;
}

uint32_t pakExtRead(const PakExt *e, int n, uint32_t off, uint8_t *dst, uint32_t len, PakSectorRead rd, void *ctx,
                    uint8_t *bounce) {
  uint32_t done = 0;
  while (done < len) {
    uint32_t at = off + done;
    const PakExt *x = pakExtFind(e, n, at);
    if (!x) break;
    uint32_t inExt = at - x->off, sec = x->sect + inExt / 512, skip = inExt % 512;
    uint32_t extLeft = x->nsect - inExt / 512;  // sectores que quedan en el tramo
    uint32_t want = len - done;
    uint32_t nsec = (skip + want + 511) / 512;
    if (nsec > 8) nsec = 8;
    if (nsec > extLeft) nsec = extLeft;
    if (!rd(sec, nsec, bounce, ctx)) break;
    uint32_t avail = nsec * 512 - skip;
    uint32_t take = want < avail ? want : avail;
    memcpy(dst + done, bounce + skip, take);
    done += take;
  }
  return done;
}
