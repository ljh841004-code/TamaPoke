#include "pak.h"
#include "pak_core.h"
#include <SD_MMC.h>
#include <FSImpl.h>
#include <Preferences.h>
#include <dirent.h>
#include <sys/stat.h>
#include <memory>
#include "sd_lock.h"
#include "sdmon.h"
#include "audio.h"
#include "mbedtls/aes.h"
#include "ff.h"

static PakAes gAes;              // clave (comprobacion y respaldo)
static mbedtls_aes_context gMb;  // el mismo AES por hardware para los datos
static bool gMbOk = false;
static PakHdr gHdr;
static PakIndex gIdx;
static uint8_t *gTbl = nullptr;  // tabla descifrada (los nombres de gIdx apuntan aqui)
static uint32_t gSize = 0;
static int8_t gState = 0;
static char gPass[48] = "";

static void loadPass() {
  static bool done = false;
  if (done) return;
  done = true;
  Preferences p;
  if (p.begin("tamapoke", true)) {
    String s = p.getString("pakpw", "");
    snprintf(gPass, sizeof(gPass), "%s", s.c_str());
    p.end();
  }
}
const char *pakPassLabel() { loadPass(); return gPass; }

static void setKeyFromPass() {
  loadPass();
  uint8_t key[16];
  pakDeriveKey(gPass[0] ? gPass : PAK_DEFAULT_PASS, key);
  gAes.setKey(key);
  if (gMbOk) mbedtls_aes_free(&gMb);
  mbedtls_aes_init(&gMb);
  gMbOk = mbedtls_aes_setkey_enc(&gMb, key, 128) == 0;
}

// XOR del flujo de clave (CTR del .pak); por hardware si se puede
static void crypt(uint32_t abs, uint8_t *buf, size_t n) {
  if (!n) return;
  if (!gMbOk) { pakCrypt(gAes, gHdr.salt, abs, buf, n); return; }
  uint8_t ctr[16], stream[16];
  uint64_t blk = abs / 16;
  memcpy(ctr, gHdr.salt, 8);
  for (int i = 0; i < 8; i++) ctr[8 + i] = (uint8_t)(blk >> (56 - 8 * i));
  size_t off = abs % 16;
  if (off) {  // empezar a mitad de bloque: el bloque actual ya "gastado" hasta off
    mbedtls_aes_crypt_ecb(&gMb, MBEDTLS_AES_ENCRYPT, ctr, stream);
    for (int i = 15; i >= 8; i--) if (++ctr[i]) break;
  }
  mbedtls_aes_crypt_ctr(&gMb, n, &off, ctr, stream, buf, buf);
}

void pakUnload() {
  gIdx.clear();
  free(gTbl);
  gTbl = nullptr;
  gSize = 0;
  gState = 0;
}

bool pakLoad() {
  pakUnload();
  uint32_t t0 = millis();
  File f = SD_MMC.open(PAK_PATH, FILE_READ);
  if (!f) return false;
  uint8_t h[PAK_HDR];
  gSize = f.size();
  if (f.read(h, PAK_HDR) != PAK_HDR || !pakParseHeader(h, gHdr) || (uint64_t)gHdr.indexOff + gHdr.indexSize > gSize ||
      gHdr.indexSize > 512UL * 1024) {
    f.close();
    gState = -2;
    Serial.println("PAK roto (cabecera)");
    return false;
  }
  setKeyFromPass();
  if (!pakKeyOk(gAes, gHdr)) {
    f.close();
    gState = -1;
    Serial.println("PAK: frase incorrecta (PAKPASS)");
    return false;
  }
  gTbl = (uint8_t *)ps_malloc(gHdr.indexSize ? gHdr.indexSize : 1);
  bool ok = gTbl && f.seek(gHdr.indexOff) && f.read(gTbl, gHdr.indexSize) == gHdr.indexSize;
  f.close();
  if (ok) {
    crypt(gHdr.indexOff, gTbl, gHdr.indexSize);
    ok = gIdx.parse(gTbl, gHdr.indexSize, gHdr.count, gSize);
  }
  if (!ok) {
    pakUnload();
    gState = -2;
    Serial.println("PAK roto (tabla)");
    return false;
  }
  gState = 1;
  Serial.printf("PAK: %u ficheros (%u MB) en %u ms\n", (unsigned)gIdx.n, (unsigned)(gSize >> 20), (unsigned)(millis() - t0));
  return true;
}

bool pakActive() { return gState == 1; }
uint32_t pakCount() { return gState == 1 ? gIdx.n : 0; }
int8_t pakState() { return gState; }

void pakSetPass(const char *pass) {
  snprintf(gPass, sizeof(gPass), "%s", pass ? pass : "");
  Preferences p;
  if (p.begin("tamapoke", false)) {
    if (gPass[0]) p.putString("pakpw", gPass); else p.remove("pakpw");
    p.end();
  }
  SdCardLock lock;
  if (lock && sdReady) pakLoad();
}

// ---- un fichero del .pak visto como un File normal (solo lectura) ----
class PakFileImpl : public fs::FileImpl {
  File base;
  uint32_t off, sz, pos = 0;
  char pth[PAK_NAME_MAX + 8];
public:
  PakFileImpl(File b, const PakEntry &e) : base(b), off(e.off), sz(e.size) {
    snprintf(pth, sizeof(pth), "/mons/%.*s", (int)e.len, e.name);
  }
  size_t write(const uint8_t *, size_t) override { return 0; }
  size_t read(uint8_t *buf, size_t n) override {
    if (pos >= sz) return 0;
    if (n > sz - pos) n = sz - pos;
    if (base.position() != off + pos && !base.seek(off + pos)) return 0;
    size_t got = base.read(buf, n);
    crypt(off + pos, buf, got);
    pos += got;
    return got;
  }
  void flush() override {}
  bool seek(uint32_t p, SeekMode mode) override {
    int64_t np = mode == SeekSet ? (int64_t)p : mode == SeekCur ? (int64_t)pos + p : (int64_t)sz + p;
    if (np < 0 || np > sz) return false;
    pos = (uint32_t)np;
    return true;
  }
  size_t position() const override { return pos; }
  size_t size() const override { return sz; }
  bool setBufferSize(size_t s) override { return base.setBufferSize(s); }
  void close() override { if (base) base.close(); }
  time_t getLastWrite() override { return 0; }
  const char *path() const override { return pth; }
  const char *name() const override { const char *s = strrchr(pth, '/'); return s ? s + 1 : pth; }
  boolean isDirectory() override { return false; }
  fs::FileImplPtr openNextFile(const char *) override { return fs::FileImplPtr(); }
  boolean seekDir(long) override { return false; }
  String getNextFileName() override { return String(); }
  String getNextFileName(bool *isDir) override { if (isDir) *isDir = false; return String(); }
  void rewindDirectory() override {}
  operator bool() override { return (bool)base; }
};

static const PakEntry *lookup(const char *path) {
  if (gState != 1 || strncmp(path, "/mons/", 6) != 0) return nullptr;
  return gIdx.find(path + 6);
}

File monsOpen(const char *path) {
  if (const PakEntry *e = lookup(path)) {
    File b = SD_MMC.open(PAK_PATH, FILE_READ);
    if (b) return File(std::make_shared<PakFileImpl>(b, *e));
  }
  return SD_MMC.open(path, FILE_READ);
}

bool monsExists(const char *path) { return lookup(path) || SD_MMC.exists(path); }

bool monsIsDir(const char *path) {
  if (gState == 1 && strncmp(path, "/mons/", 6) == 0) {
    char pre[PAK_NAME_MAX + 2];
    snprintf(pre, sizeof(pre), "%s/", path + 6);
    if (gIdx.hasPrefix(pre)) return true;
  }
  File d = SD_MMC.open(path);
  bool dir = d && d.isDirectory();
  if (d) d.close();
  return dir;
}

void pakForEach(void (*cb)(const char *, uint32_t, void *), void *ctx) {
  if (gState != 1) return;
  char nm[PAK_NAME_MAX + 1];
  for (uint32_t i = 0; i < gIdx.n; i++) {
    snprintf(nm, sizeof(nm), "%.*s", (int)gIdx.e[i].len, gIdx.e[i].name);
    cb(nm, gIdx.e[i].size, ctx);
  }
}

bool monsForEachName(void (*cb)(const char *, void *), void *ctx) {
  SdCardLock lock(pdMS_TO_TICKS(3000));
  if (!lock || !sdReady) return false;
  if (gState == 1) {
    char nm[PAK_NAME_MAX + 1];
    for (uint32_t i = 0; i < gIdx.n; i++) {
      snprintf(nm, sizeof(nm), "%.*s", (int)gIdx.e[i].len, gIdx.e[i].name);
      cb(nm, ctx);
    }
  }
  static const char *const DIRS[2] = { "", "fx/" };
  for (const char *sub : DIRS) {
    char dir[32];
    snprintf(dir, sizeof(dir), "/sdcard/mons/%s", sub);
    DIR *d = opendir(dir);
    if (!d) continue;
    struct dirent *e;
    char rel[64];
    while ((e = readdir(d)) != nullptr) {
      if (e->d_type == DT_DIR || e->d_name[0] == '.') continue;
      snprintf(rel, sizeof(rel), "%s%.59s", sub, e->d_name);
      cb(rel, ctx);
    }
    closedir(d);
  }
  return true;
}

// ---------------------------------------------------------------- empaquetar en el aparato
struct BItem { char name[PAK_NAME_MAX + 1]; uint32_t size; };

static bool skipName(const char *n) {
  if (n[0] == '.') return true;
  size_t l = strlen(n);
  if (l > 4 && !strcmp(n + l - 4, ".tmp")) return true;
  return !strcmp(n, "update.bin") || !strcmp(n, "update_done.bin");
}

// ko12.8.1: nombres y tamanos en UNA pasada con FatFs (f_readdir ya trae el tamano). Con stat()
// por fichero, FatFs recorre la carpeta desde el principio cada vez: con ~1300 ficheros en /mons y
// la SD en modo 1-bit, abrir la pantalla de 묶기 tardaba minutos y parecia colgado
static bool fatListMons(BItem *v, uint32_t &n, uint32_t cap) {
  static FILINFO fi;  // ~270 bytes: fuera de la pila (solo la usa el bucle principal)
  for (int drv = 0; drv < FF_VOLUMES; drv++) {
    char base[12];
    snprintf(base, sizeof(base), "%d:/mons", drv);
    FF_DIR d;
    if (f_opendir(&d, base) != FR_OK) continue;  // unidad sin montar o sin carpeta mons
    f_closedir(&d);
    const char *sub[8] = { "" };
    char subBuf[7][24];
    int ns = 1;
    bool ok = true;
    n = 0;
    for (int s = 0; s < ns && ok; s++) {
      char dir[48];
      snprintf(dir, sizeof(dir), "%s/%s", base, sub[s]);
      size_t dl = strlen(dir);
      if (dl > 1 && dir[dl - 1] == '/') dir[dl - 1] = 0;
      if (f_opendir(&d, dir) != FR_OK) { if (s == 0) ok = false; continue; }
      FRESULT r;
      while ((r = f_readdir(&d, &fi)) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & AM_DIR) {
          if (s == 0 && fi.fname[0] != '.' && ns < 8 && strlen(fi.fname) < 20) {
            snprintf(subBuf[ns - 1], sizeof(subBuf[0]), "%.19s/", fi.fname);  // strlen < 20 (arriba)
            sub[ns] = subBuf[ns - 1];
            ns++;
          }
          continue;
        }
        if (n >= cap || skipName(fi.fname)) continue;
        char rel[64];
        if (snprintf(rel, sizeof(rel), "%s%s", sub[s], fi.fname) > PAK_NAME_MAX) continue;
        snprintf(v[n].name, sizeof(v[n].name), "%.40s", rel);  // <= PAK_NAME_MAX (arriba)
        v[n].size = (uint32_t)fi.fsize;
        n++;
      }
      if (r != FR_OK) ok = false;
      f_closedir(&d);
    }
    if (ok) return true;
  }
  n = 0;
  return false;
}

static int cmpItem(const void *a, const void *b) {
  const BItem *x = (const BItem *)a, *y = (const BItem *)b;
  size_t lx = strlen(x->name), ly = strlen(y->name);
  int c = memcmp(x->name, y->name, lx < ly ? lx : ly);
  return c ? c : (int)lx - (int)ly;
}

// lista /mons y sus subcarpetas (un nivel: fx/), ordenada por nombre
static BItem *listMons(uint32_t &n) {
  n = 0;
  uint32_t cap = 2048;
  BItem *v = (BItem *)ps_malloc(sizeof(BItem) * cap);
  if (!v) return nullptr;
  uint32_t t0 = millis();
  if (fatListMons(v, n, cap)) {
    qsort(v, n, sizeof(BItem), cmpItem);
    Serial.printf("PAK lista: %u ficheros en %u ms\n", (unsigned)n, (unsigned)(millis() - t0));
    return v;
  }
  Serial.println("PAK lista: FatFs no, stat() (lento)");
  const char *sub[8] = { "" };
  int ns = 1;
  char subBuf[7][24];
  {
    DIR *d = opendir("/sdcard/mons");
    if (!d) { free(v); return nullptr; }
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
      if (e->d_type == DT_DIR && e->d_name[0] != '.' && ns < 8 && strlen(e->d_name) < 20) {
        snprintf(subBuf[ns - 1], sizeof(subBuf[0]), "%.19s/", e->d_name);
        sub[ns] = subBuf[ns - 1];
        ns++;
      }
    }
    closedir(d);
  }
  for (int s = 0; s < ns; s++) {
    char dir[48];
    snprintf(dir, sizeof(dir), "/sdcard/mons/%s", sub[s]);
    DIR *d = opendir(dir);
    if (!d) continue;
    struct dirent *e;
    while ((e = readdir(d)) != nullptr && n < cap) {
      if (e->d_type == DT_DIR || skipName(e->d_name)) continue;
      char rel[64];
      if (snprintf(rel, sizeof(rel), "%s%s", sub[s], e->d_name) > PAK_NAME_MAX) continue;
      char full[96];
      snprintf(full, sizeof(full), "/sdcard/mons/%s", rel);
      struct stat st;
      if (stat(full, &st) != 0) continue;
      snprintf(v[n].name, sizeof(v[n].name), "%.40s", rel);  // <= PAK_NAME_MAX (arriba)
      v[n].size = (uint32_t)st.st_size;
      n++;
    }
    closedir(d);
  }
  qsort(v, n, sizeof(BItem), cmpItem);
  Serial.printf("PAK lista: %u ficheros en %u ms (stat)\n", (unsigned)n, (unsigned)(millis() - t0));
  return v;
}

bool pakScan(PakBuildInfo &out) {
  out = PakBuildInfo();
  SdCardLock lock(pdMS_TO_TICKS(3000));
  if (!lock || !sdReady) return false;
  uint32_t n;
  BItem *v = listMons(n);
  if (!v) return false;
  out.files = n;
  for (uint32_t i = 0; i < n; i++) out.bytes += v[i].size;
  free(v);
  return true;
}

uint8_t pakBuild(void (*progress)(uint64_t, uint64_t, uint32_t, uint32_t)) {
  if (!sdReady) return 2;
  if (!audioPauseForUpload()) { audioResumeAfterUpload(); return 2; }
  struct ResumeAudio { ~ResumeAudio() { audioResumeAfterUpload(); } } resumeAudio;
  uint32_t n;
  BItem *v;
  uint64_t total = 0;
  {
    SdCardLock lock;
    if (!lock) return 2;
    v = listMons(n);
    if (!v) return 2;
    if (!n) { free(v); return 3; }
    for (uint32_t i = 0; i < n; i++) total += v[i].size;
    uint64_t freeB = SD_MMC.totalBytes() - SD_MMC.usedBytes();
    if (total + (uint64_t)n * 64 + (1u << 20) > freeB || total > 0xF0000000ull) { free(v); return 1; }
    pakUnload();  // se va a reemplazar
    SD_MMC.remove("/mons.pak.tmp");
  }
  setKeyFromPass();
  memset(&gHdr, 0, sizeof(gHdr));
  gHdr.dataOff = PAK_HDR;
  for (int i = 0; i < 8; i++) gHdr.salt[i] = (uint8_t)esp_random();
  uint8_t *buf = (uint8_t *)malloc(16384);
  uint8_t *tbl = (uint8_t *)ps_malloc((size_t)n * (9 + PAK_NAME_MAX));
  if (!buf || !tbl) { free(buf); free(tbl); free(v); return 2; }
  bool ok = true;
  uint32_t pos = PAK_HDR, tp = 0;
  uint64_t done = 0;
  File out;
  {
    SdCardLock lock;
    out = SD_MMC.open("/mons.pak.tmp", FILE_WRITE);
    memset(buf, 0, PAK_HDR);
    ok = out && out.write(buf, PAK_HDR) == PAK_HDR;
  }
  for (uint32_t i = 0; ok && i < n; i++) {
    char path[64];
    snprintf(path, sizeof(path), "/mons/%s", v[i].name);
    File in;
    {
      SdCardLock lock;
      in = SD_MMC.open(path, FILE_READ);
    }
    if (!in) { ok = false; break; }
    uint32_t sz = v[i].size, got = 0;
    while (ok && got < sz) {
      size_t want = sz - got > 16384 ? 16384 : sz - got;
      SdCardLock lock;
      if (in.read(buf, want) != want) { ok = false; break; }
      crypt(pos + got, buf, want);
      if (out.write(buf, want) != want) { ok = false; break; }
      got += want;
      done += want;
    }
    {
      SdCardLock lock;
      in.close();
    }
    uint8_t l = (uint8_t)strlen(v[i].name);
    uint8_t *r = tbl + tp;
    r[0] = (uint8_t)pos; r[1] = (uint8_t)(pos >> 8); r[2] = (uint8_t)(pos >> 16); r[3] = (uint8_t)(pos >> 24);
    r[4] = (uint8_t)sz; r[5] = (uint8_t)(sz >> 8); r[6] = (uint8_t)(sz >> 16); r[7] = (uint8_t)(sz >> 24);
    r[8] = l;
    memcpy(r + 9, v[i].name, l);
    tp += 9 + l;
    pos += sz;
    if (progress && (i % 4 == 0 || i + 1 == n)) progress(done, total, i + 1, n);
    delay(1);
  }
  if (ok) {
    SdCardLock lock;
    gHdr.count = n;
    gHdr.indexOff = pos;
    gHdr.indexSize = tp;
    crypt(pos, tbl, tp);
    ok = out.write(tbl, tp) == tp;
    pakMakeCheck(gAes, gHdr.check);
    uint8_t h[PAK_HDR];
    pakWriteHeader(gHdr, h);
    ok = ok && out.seek(0) && out.write(h, PAK_HDR) == PAK_HDR;
  }
  {
    SdCardLock lock;
    if (out) out.close();
    if (ok) {
      SD_MMC.remove(PAK_PATH);
      ok = SD_MMC.rename("/mons.pak.tmp", PAK_PATH);
    } else {
      SD_MMC.remove("/mons.pak.tmp");
    }
    sdForgetMissing();
    pakLoad();  // ya se lee del nuevo
  }
  free(buf);
  free(tbl);
  free(v);
  return ok ? 0 : 2;
}
