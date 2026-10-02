#include "fxanim.h"
#include "pin_config.h"
#include "sdmon.h"
#include "sd_lock.h"
#include <FS.h>
#include <SD_MMC.h>
#include "battle.h"

// ko11.31: cache de efectos ya leidos (se llena al empezar el combate, nunca a mitad de un
// golpe: leer de la SD mientras suena la musica la hacia cortarse)
#define FX_CACHE_N 10
#define FX_BUDGET (3UL * 1024 * 1024)
static FxAnim fxC[FX_CACHE_N];

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline int16_t rds16(const uint8_t *p) { return (int16_t)rd16(p); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

#define FX_HDR 8          // "TFX2" u8 lados u16 ms u8 nfondos
#define FX_BGHDR 14       // u8 fondo i16 bx by u16 bw bh sx sy u8 alfa
#define FX_CLIP_Y 262     // el cuadro de texto queda limpio

void FxAnim::unload() {
  if (data) free(data);
  data = nullptr;
  size = 0;
  key = 0xFFFF;
  mid = 0;
}

static bool fxParse(FxAnim &a, File &f);

// abre el archivo del movimiento: mons/fx/<n> o suelto en mons/ (el instalador web)
static File fxOpen(uint8_t id) {
  char name[16], path[28];
  uint8_t t, s, v;
  if (moveDecode(id, &t, &s, &v)) snprintf(name, sizeof(name), "f%02u%u%u.bin", t, s, v);
  else snprintf(name, sizeof(name), "m%03u.bin", id);
  snprintf(path, sizeof(path), "/mons/fx/%s", name);
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    snprintf(path, sizeof(path), "/mons/%s", name);
    f = SD_MMC.open(path, FILE_READ);
  }
  return f;
}

const FxAnim *fxFind(uint8_t id) {
  if (!id) return nullptr;
  for (FxAnim &a : fxC) if (a.isId(id)) return &a;
  return nullptr;
}

void fxPreload(const uint8_t *ids, uint8_t n) {
  if (!sdReady) return;
  auto wanted = [&](uint8_t id) { for (uint8_t i = 0; i < n; i++) if (ids[i] == id) return true; return false; };
  uint32_t used = 0;
  for (FxAnim &a : fxC) {
    if (a.ok() && !wanted(a.mid)) a.unload();  // lo de antes que ya no hace falta
    if (a.ok()) used += a.size;
  }
  for (uint8_t i = 0; i < n; i++) {
    uint8_t id = ids[i];
    if (!id || id == MOVE_STRUGGLE || fxFind(id)) continue;
    FxAnim *slot = nullptr;
    for (FxAnim &a : fxC) if (!a.ok()) { slot = &a; break; }
    if (!slot) return;
    uint32_t sz = 0;
    {
      SdCardLock lock;
      if (!lock) return;
      File f = fxOpen(id);
      if (!f) continue;
      sz = f.size();
      f.close();
    }
    if (used + sz > FX_BUDGET) continue;  // no cabe: ese usara el efecto dibujado
    if (slot->loadId(id)) used += slot->size;
  }
}
// ko11.31: los de tipo (fTTSV) y placaje / los de estado (mNNN.bin), por id
bool FxAnim::loadId(uint8_t id) {
  uint8_t t, s, v;
  uint16_t k = moveDecode(id, &t, &s, &v) ? (uint16_t)(t * 9 + s * 3 + v) : (uint16_t)(1000 + id);
  if (data && key == k) { mid = id; return true; }
  unload();
  if (!sdReady || !id) return false;
  File f;
  {
    SdCardLock lock;
    if (!lock) return false;
    f = fxOpen(id);
  }
  if (!f) return false;
  if (!fxParse(*this, f)) return false;
  key = k;
  mid = id;
  return true;
}

bool FxAnim::load(uint8_t type, uint8_t tier, uint8_t var) {
  return loadId(moveIdTyped(type, tier, var));
}

static bool fxParse(FxAnim &a, File &f) {
  uint32_t sz;
  {
    SdCardLock lock;
    if (!lock) { f.close(); return false; }
    sz = f.size();
  }
  if (sz < FX_HDR + 4 || sz > 3UL * 1024 * 1024) { SdCardLock l; f.close(); return false; }
  a.data = (uint8_t *)ps_malloc(sz);
  // a trozos, soltando la SD entre uno y otro: la musica (que tambien lee de la SD) no se corta
  bool ok = a.data != nullptr;
  for (uint32_t off = 0; ok && off < sz;) {
    uint32_t n = sz - off > 8192 ? 8192 : sz - off;
    {
      SdCardLock lock;
      ok = lock && f.read(a.data + off, n) == n;
    }
    off += n;
    delay(1);
  }
  {
    SdCardLock l;
    f.close();
  }
  if (!ok || memcmp(a.data, "TFX2", 4) != 0 || a.data[4] < 1 || a.data[4] > 2) {
    a.unload();
    return false;
  }
  a.size = sz;
  a.frameMs = rd16(a.data + 5);
  if (a.frameMs < 10) a.frameMs = 10;
  // valida los fondos y las tablas de los lados
  const uint8_t *p = a.data + FX_HDR, *end = a.data + sz;
  for (uint8_t i = 0; i < a.data[7]; i++) {
    if (p + 6 > end) { a.unload(); return false; }
    uint32_t w = rd16(p), h = rd16(p + 2), np = rd16(p + 4);
    if (!w || !h || np > 256) { a.unload(); return false; }
    p += 6 + np * 3 + w * h;
  }
  for (uint8_t s = 0; s < a.data[4]; s++) {
    if (p + 2 > end) { a.unload(); return false; }
    uint16_t n = rd16(p);
    if (p + 2 + 4UL * n > end) { a.unload(); return false; }
    for (uint16_t i = 0; i < n; i++)
      if (rd32(p + 2 + 4 * i) + FX_BGHDR + 13 > sz) { a.unload(); return false; }
    p += 2 + 4UL * n;
  }
  return true;
}

const uint8_t *FxAnim::bg(uint8_t i) const {
  const uint8_t *p = data + FX_HDR;
  for (uint8_t j = 0; j < data[7]; j++) {
    if (j == i) return p;
    p += 6 + rd16(p + 4) * 3 + (uint32_t)rd16(p) * rd16(p + 2);
  }
  return nullptr;
}

const uint8_t *FxAnim::table(uint8_t side) const {
  if (side >= data[4]) side = 0;
  const uint8_t *p = data + FX_HDR;
  for (uint8_t j = 0; j < data[7]; j++) p += 6 + rd16(p + 4) * 3 + (uint32_t)rd16(p) * rd16(p + 2);
  for (uint8_t s = 0; s < side; s++) p += 2 + 4UL * rd16(p);
  return p;
}

uint16_t FxAnim::frames(uint8_t side) const { return data ? rd16(table(side)) : 0; }

const uint8_t *FxAnim::frame(uint8_t side, uint32_t t) const {
  if (!data) return nullptr;
  const uint8_t *p = table(side);
  uint16_t n = rd16(p);
  uint32_t i = t / frameMs;
  if (i >= n) return nullptr;
  return data + rd32(p + 2 + 4 * i);
}

// mezcla normal: c * a + fondo * (32 - a), a de 0..32
static inline uint16_t mix565(uint16_t c, uint16_t b, uint32_t a) {
  uint32_t ia = 32 - a;
  uint32_t r = (((c >> 11) & 31) * a + ((b >> 11) & 31) * ia) >> 5;
  uint32_t g = (((c >> 5) & 63) * a + ((b >> 5) & 63) * ia) >> 5;
  uint32_t bl = ((c & 31) * a + (b & 31) * ia) >> 5;
  return (uint16_t)(r << 11 | g << 5 | bl);
}

// premultiplicado: c + fondo * (32 - a), saturando (sirve para la luz aditiva)
static inline uint16_t addPre565(uint16_t c, uint16_t b, uint32_t a) {
  uint32_t ia = 32 - a;
  uint32_t r = ((c >> 11) & 31) + ((((b >> 11) & 31) * ia) >> 5);
  uint32_t g = ((c >> 5) & 63) + ((((b >> 5) & 63) * ia) >> 5);
  uint32_t bl = (c & 31) + (((b & 31) * ia) >> 5);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (bl > 31) bl = 31;
  return (uint16_t)(r << 11 | g << 5 | bl);
}

void FxAnim::drawBg(uint16_t *fb, uint8_t side, uint32_t t, int dx, int dy) const {
  const uint8_t *f = frame(side, t);
  if (!fb || !f || f[0] == 0xFF || f[13] == 0) return;
  const uint8_t *b = bg(f[0]);
  if (!b) return;
  int bx = rds16(f + 1) + dx, by = rds16(f + 3) + dy;
  int bw = rd16(f + 5), bh = rd16(f + 7);
  uint32_t sx = rd16(f + 9), sy = rd16(f + 11), al = f[13];
  if (!sx || !sy) return;
  uint32_t w = rd16(b), h = rd16(b + 2), np = rd16(b + 4);
  const uint8_t *pal = b + 6, *px = b + 6 + np * 3;
  int x0 = bx < 0 ? 0 : bx, x1 = bx + bw > LCD_WIDTH ? LCD_WIDTH : bx + bw;
  int y0 = by < 0 ? 0 : by, y1 = by + bh > FX_CLIP_Y ? FX_CLIP_Y : by + bh;
  if (x0 >= x1 || y0 >= y1) return;
  // paso en 16.16 por pixel de pantalla (escala en 8.8)
  uint32_t incX = (65536UL * 256) / sx, incY = (65536UL * 256) / sy;
  uint32_t wrapX = w << 16;
  for (int y = y0; y < y1; y++) {
    uint32_t v = ((uint32_t)(y - by) * incY >> 16) % h;
    const uint8_t *row = px + v * w;
    uint32_t u = ((uint32_t)(x0 - bx) * incX) % wrapX;
    uint16_t *d = fb + y * LCD_WIDTH + x0;
    for (int x = x0; x < x1; x++, d++) {
      uint8_t ix = row[u >> 16];
      const uint8_t *e = pal + (ix < np ? ix : 0) * 3;
      uint32_t a = e[2] * al >> 5;
      if (a) *d = a >= 32 ? rd16(e) : mix565(rd16(e), *d, a);
      u += incX;
      if (u >= wrapX) u -= wrapX;
    }
  }
}

void FxAnim::drawFg(uint16_t *fb, uint8_t side, uint32_t t, int dx, int dy) const {
  const uint8_t *f = frame(side, t);
  if (!fb || !f) return;
  f += FX_BGHDR;
  int x0 = rds16(f) + dx, y0 = rds16(f + 2) + dy;
  int w = rd16(f + 4), h = rd16(f + 6);
  uint8_t np = f[8];
  if (!w || !h) return;
  const uint8_t *pal = f + 9;
  const uint8_t *p = pal + np * 3;
  if (p + 4 > data + size) return;
  uint32_t len = rd32(p);
  p += 4;
  const uint8_t *end = p + len;
  if (end > data + size) return;
  uint32_t n = (uint32_t)w * h, i = 0;
  while (i < n && p < end) {
    uint8_t c = *p++;
    if (c < 128) { i += c + 1u; continue; }
    uint32_t run = c - 127u;
    for (uint32_t k = 0; k < run && i < n && p < end; k++, i++) {
      uint8_t ix = *p++;
      int x = x0 + (int)(i % w), y = y0 + (int)(i / w);
      if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= FX_CLIP_Y || ix >= np) continue;
      const uint8_t *e = pal + ix * 3;
      uint16_t *d = fb + y * LCD_WIDTH + x;
      *d = addPre565(rd16(e), *d, e[2]);
    }
  }
}
