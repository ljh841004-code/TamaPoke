// ko11.6: copia de la partida en la SD (ver savebak.h)
#include "savebak.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "sd_lock.h"
#include "sdmon.h"
#include "box.h"

static const char *const BAK_PATH[2] = { "/tpsave/save0.bak", "/tpsave/save1.bak" };
// que se copia: la partida y la caja/salon/liga/pokedex. La WiFi ("tpnet") NO
struct BakNs { const char *ns; bool big; };
static const BakNs BAK_NS[] = {
  { "tamapoke", false }, { "tpbox", true }, { "tphall", true }, { "tpfame", true }, { "tpdex", true },
};

static const char *partFor(bool big) { return (big && bigPart()) ? bigPart() : NVS_DEFAULT_PART_NAME; }

static uint8_t *bakBuf() {
  static uint8_t *b = nullptr;
  if (!b) b = (uint8_t *)ps_malloc(BAK_MAX_BYTES);
  return b;
}

// lee una ranura entera y la comprueba (sin lock: lo pone quien llama)
static size_t readSlot(uint8_t i, uint8_t *buf, BakHdr *h) {
  File f = SD_MMC.open(BAK_PATH[i], FILE_READ);
  if (!f) return 0;
  size_t sz = f.size();
  if (sz < sizeof(BakHdr) + 4 || sz > BAK_MAX_BYTES) { f.close(); return 0; }
  size_t got = f.read(buf, sz);
  f.close();
  if (got != sz || !bakParse(buf, sz, h)) return 0;
  return sz;
}

void bakInfo(BakSlot out[2]) {
  out[0].ok = out[1].ok = false;
  uint8_t *buf = bakBuf();
  if (!sdReady || !buf) return;
  SdCardLock lock;
  if (!lock) return;
  for (uint8_t i = 0; i < 2; i++) out[i].ok = readSlot(i, buf, &out[i].h) > 0;
}

int bakNewest(const BakSlot s[2]) {
  if (s[0].ok && s[1].ok) return s[1].h.seq > s[0].h.seq ? 1 : 0;
  return s[0].ok ? 0 : s[1].ok ? 1 : -1;
}

// una entrada de la NVS al escritor, segun su tipo
static bool addEntry(BakWriter &w, nvs_handle_t h, const char *ns, const nvs_entry_info_t &e) {
  switch (e.type) {
    case NVS_TYPE_U8:  { uint8_t v;  return nvs_get_u8(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 1); }
    case NVS_TYPE_I8:  { int8_t v;   return nvs_get_i8(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 1); }
    case NVS_TYPE_U16: { uint16_t v; return nvs_get_u16(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 2); }
    case NVS_TYPE_I16: { int16_t v;  return nvs_get_i16(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 2); }
    case NVS_TYPE_U32: { uint32_t v; return nvs_get_u32(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 4); }
    case NVS_TYPE_I32: { int32_t v;  return nvs_get_i32(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 4); }
    case NVS_TYPE_U64: { uint64_t v; return nvs_get_u64(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 8); }
    case NVS_TYPE_I64: { int64_t v;  return nvs_get_i64(h, e.key, &v) == ESP_OK && w.add(ns, e.key, e.type, &v, 8); }
    case NVS_TYPE_STR: {
      char s[128];
      size_t len = sizeof(s);
      if (nvs_get_str(h, e.key, s, &len) != ESP_OK) return false;
      return w.add(ns, e.key, e.type, s, len);  // len incluye el 0 final
    }
    case NVS_TYPE_BLOB: {
      static uint8_t blob[4096];
      size_t len = sizeof(blob);
      if (nvs_get_blob(h, e.key, blob, &len) != ESP_OK) return false;
      return w.add(ns, e.key, e.type, blob, len);
    }
    default: return true;  // tipos raros: se saltan
  }
}

bool bakBackupNow(int16_t dex, uint16_t lvl, uint32_t epoch, bool manual) {
  uint8_t *buf = bakBuf();
  if (!sdReady || !buf) return false;
  BakSlot s[2];
  bakInfo(s);
  int nw = bakNewest(s);
  uint8_t slot = nw < 0 ? 0 : (uint8_t)(1 - nw);  // se pisa la mas vieja: la otra queda
  uint32_t seq = nw < 0 ? 1 : s[nw].h.seq + 1;
  BakWriter w(buf, BAK_MAX_BYTES);
  w.begin(seq, epoch, dex, lvl, manual ? BAKF_MANUAL : 0);
  for (const BakNs &b : BAK_NS) {
    const char *part = partFor(b.big);
    nvs_handle_t h;
    if (nvs_open_from_partition(part, b.ns, NVS_READONLY, &h) != ESP_OK) continue;  // espacio vacio
    nvs_iterator_t it = nullptr;
    esp_err_t r = nvs_entry_find(part, b.ns, NVS_TYPE_ANY, &it);
    bool ok = true;
    while (r == ESP_OK && ok) {
      nvs_entry_info_t e;
      nvs_entry_info(it, &e);
      ok = addEntry(w, h, b.ns, e);
      r = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(h);
    if (!ok) { Serial.printf("BAK: %s no cabe\n", b.ns); return false; }
  }
  size_t n = w.finish();
  if (!n) return false;
  SdCardLock lock;
  if (!lock) return false;
  SD_MMC.mkdir("/tpsave");
  SD_MMC.remove(BAK_PATH[slot]);  // FILE_WRITE anade al final: empezar de cero
  File f = SD_MMC.open(BAK_PATH[slot], FILE_WRITE);
  if (!f) return false;
  size_t put = f.write(buf, n);
  f.close();
  // se relee y se comprueba: una copia mala no debe pasar por buena
  BakHdr h;
  bool ok = put == n && readSlot(slot, buf, &h) == n && h.seq == seq;
  Serial.printf("BAK: ranura %u seq %u %u bytes %s\n", slot, (unsigned)seq, (unsigned)n, ok ? "ok" : "FALLO");
  return ok;
}

// restaurar: cada espacio se vacia una vez y se escribe con lo de la copia
struct RestoreCtx { char erased[8][16]; uint8_t nErased; bool ok; };
static const char *nsPart(const char *ns) {
  for (const BakNs &b : BAK_NS)
    if (!strcmp(b.ns, ns)) return partFor(b.big);
  return nullptr;
}
static bool restoreRec(void *vctx, const char *ns, const char *key, uint8_t type, const uint8_t *d, size_t len) {
  RestoreCtx *c = (RestoreCtx *)vctx;
  const char *part = nsPart(ns);
  if (!part) return true;  // espacio que ya no existe: se ignora
  nvs_handle_t h;
  if (nvs_open_from_partition(part, ns, NVS_READWRITE, &h) != ESP_OK) return c->ok = false;
  bool fresh = true;
  for (uint8_t i = 0; i < c->nErased; i++)
    if (!strcmp(c->erased[i], ns)) fresh = false;
  if (fresh && c->nErased < 8) {
    nvs_erase_all(h);
    strncpy(c->erased[c->nErased], ns, 15);
    c->erased[c->nErased++][15] = 0;
  }
  esp_err_t r = ESP_OK;
  switch (type) {
    case NVS_TYPE_U8:  if (len == 1) r = nvs_set_u8(h, key, d[0]); break;
    case NVS_TYPE_I8:  if (len == 1) r = nvs_set_i8(h, key, (int8_t)d[0]); break;
    case NVS_TYPE_U16: { uint16_t v; if (len == 2) { memcpy(&v, d, 2); r = nvs_set_u16(h, key, v); } break; }
    case NVS_TYPE_I16: { int16_t v;  if (len == 2) { memcpy(&v, d, 2); r = nvs_set_i16(h, key, v); } break; }
    case NVS_TYPE_U32: { uint32_t v; if (len == 4) { memcpy(&v, d, 4); r = nvs_set_u32(h, key, v); } break; }
    case NVS_TYPE_I32: { int32_t v;  if (len == 4) { memcpy(&v, d, 4); r = nvs_set_i32(h, key, v); } break; }
    case NVS_TYPE_U64: { uint64_t v; if (len == 8) { memcpy(&v, d, 8); r = nvs_set_u64(h, key, v); } break; }
    case NVS_TYPE_I64: { int64_t v;  if (len == 8) { memcpy(&v, d, 8); r = nvs_set_i64(h, key, v); } break; }
    case NVS_TYPE_STR: if (len && d[len - 1] == 0) r = nvs_set_str(h, key, (const char *)d); break;
    case NVS_TYPE_BLOB: r = nvs_set_blob(h, key, d, len); break;
    default: break;
  }
  if (r == ESP_OK) r = nvs_commit(h);
  nvs_close(h);
  if (r != ESP_OK) c->ok = false;
  return c->ok;
}

bool bakRestore(uint8_t slot) {
  uint8_t *buf = bakBuf();
  if (!sdReady || !buf || slot > 1) return false;
  size_t n;
  BakHdr h;
  {
    SdCardLock lock;
    if (!lock) return false;
    n = readSlot(slot, buf, &h);  // CRC comprobado ANTES de tocar nada
  }
  if (!n) return false;
  RestoreCtx c;
  memset(&c, 0, sizeof(c));
  c.ok = true;
  bool ok = bakParse(buf, n, &h, restoreRec, &c) && c.ok;
  Serial.printf("BAK: restaurar ranura %u %s\n", slot, ok ? "ok" : "FALLO");
  return ok;
}
