// Lo comun del TamaPoke para movil (Android) y su prueba en el PC (android/test): el sketch, la "NVS"
// en un fichero y la importacion de mons.pak. host.cpp (Android) y test/desktop_check.cpp lo incluyen.
#include "sketch.cpp"
#include "../../TamaPoke/pak_core.h"
#include <atomic>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

extern const char *gSdRoot;
extern std::atomic<int64_t> gHostPausedUs;
extern int32_t gHostClockAdj;
static std::string gFiles, gSdDir;

// ---------------------------------------------------------------- "NVS" en fichero
// el almacen de Preferences (prefs_impl.inc, en host_stubs.cpp) se vuelca entero: [ns][clave][valor]
void hostPrefsDump(std::vector<uint8_t> &out);
void hostPrefsLoad(const uint8_t *d, size_t n);
static std::vector<uint8_t> gPrefsLast;
static void prefsLoadFile() {
  FILE *f = fopen((gFiles + "/nvs.bin").c_str(), "rb");
  if (!f) return;
  std::vector<uint8_t> d;
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
  fclose(f);
  hostPrefsLoad(d.data(), d.size());
  gPrefsLast = d;
}
static void prefsSaveFile(bool force) {
  std::vector<uint8_t> d;
  hostPrefsDump(d);
  if (!force && d == gPrefsLast) return;
  std::string tmp = gFiles + "/nvs.tmp", fin = gFiles + "/nvs.bin";
  FILE *f = fopen(tmp.c_str(), "wb");
  if (!f) return;
  bool ok = fwrite(d.data(), 1, d.size(), f) == d.size();
  ok = fflush(f) == 0 && ok;
  fsync(fileno(f));
  fclose(f);
  if (ok && rename(tmp.c_str(), fin.c_str()) == 0) gPrefsLast.swap(d);
}
void hostSaveClockAdj() {
  FILE *f = fopen((gFiles + "/clock_adj.txt").c_str(), "w");
  if (f) { fprintf(f, "%d\n", (int)gHostClockAdj); fclose(f); }
}
static void loadClockAdj() {
  FILE *f = fopen((gFiles + "/clock_adj.txt").c_str(), "r");
  if (!f) return;
  int v = 0;
  if (fscanf(f, "%d", &v) == 1) gHostClockAdj = v;
  fclose(f);
}

// ---------------------------------------------------------------- importar mons.pak
void hostPumpEvents();  // Android: atender al sistema (el PC no hace nada)
static void progressScreen(const char *l1, const char *l2, uint32_t done, uint32_t total) {
  hostPumpEvents();
  uiScreenBg();
  drawFit(l1, 150, 380, UI_INK, 2);
  if (l2 && *l2) drawFit(l2, 190, 380, UI_INK, 1);
  if (total) {
    gfx->fillRoundRect(83, 240, 300, 22, 11, 0xC618);
    int w = (int)(300.0 * done / total);
    if (w > 0) gfx->fillRoundRect(83, 240, w < 22 ? 22 : w, 22, 11, UI_BAR_OK);
    char p[16];
    snprintf(p, sizeof(p), "%u%%", (unsigned)((uint64_t)done * 100 / total));
    drawFit(p, 280, 200, UI_INK, 2);
  }
  gfx->flush();
}

static bool rmTree(const std::string &p) {
  DIR *d = opendir(p.c_str());
  if (!d) return unlink(p.c_str()) == 0;
  while (struct dirent *e = readdir(d)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    rmTree(p + "/" + e->d_name);
  }
  closedir(d);
  return rmdir(p.c_str()) == 0;
}

// mons.pak entero o en trozos (mons.pak.001, .002 ... elegidos juntos): se leen como un solo fichero
struct PakSrc {
  std::vector<int> fd;
  std::vector<uint64_t> start, size;
  uint64_t total = 0;
  bool add(int f) {
    struct stat st;
    if (fstat(f, &st) != 0) return false;
    fd.push_back(f); start.push_back(total); size.push_back((uint64_t)st.st_size);
    total += (uint64_t)st.st_size;
    return true;
  }
  bool read(uint8_t *b, size_t n, uint64_t at) const {
    while (n) {
      size_t i = 0;
      while (i < fd.size() && at >= start[i] + size[i]) i++;
      if (i >= fd.size()) return false;
      uint64_t off = at - start[i];
      size_t k = (size_t)std::min<uint64_t>(n, size[i] - off);
      ssize_t r = pread(fd[i], b, k, (off_t)off);
      if (r <= 0) return false;
      b += r; n -= r; at += r;
    }
    return true;
  }
};

// devuelve "" si fue bien, o el motivo
static bool gImportMerged = false;  // el ultimo paquete se añadio (musica) en vez de sustituir
static std::string importPak(const std::vector<int> &fds) {
  PakSrc src;
  for (int f : fds)
    if (!src.add(f)) return "파일을 읽을 수 없어요";
  if (src.total > 0xFFFFFFFFull) return "파일이 너무 커요";
  uint32_t fsize = (uint32_t)src.total;
  uint8_t h[PAK_HDR];
  if (!src.read(h, PAK_HDR, 0)) return "파일이 너무 작아요";
  PakHdr hdr;
  if (!pakParseHeader(h, hdr)) return fds.size() > 1 ? "조각을 모두 골랐는지 확인해 주세요" : "mons.pak 파일이 아니에요 (조각이면 모두 함께 골라요)";
  uint8_t key[16];
  pakDeriveKey(PAK_DEFAULT_PASS, key);
  PakAes aes;
  aes.setKey(key);
  if (!pakKeyOk(aes, hdr)) return "다른 암호로 만든 mons.pak이에요";
  if ((uint64_t)hdr.indexOff + hdr.indexSize > fsize) return "조각이 모자라거나 파일이 깨졌어요";
  std::vector<uint8_t> tbl(hdr.indexSize);
  if (!src.read(tbl.data(), tbl.size(), hdr.indexOff)) return "목록을 읽을 수 없어요";
  pakCrypt(aes, hdr.salt, hdr.indexOff, tbl.data(), tbl.size());
  PakIndex idx;
  if (!idx.parse(tbl.data(), hdr.indexSize, hdr.count, fsize)) return "mons.pak 목록이 깨졌어요";
  uint64_t total = 0, done = 0;
  for (uint32_t i = 0; i < idx.n; i++) total += idx.e[i].size;
  std::string tmp = gSdDir + "/mons_new", fin = gSdDir + "/mons";
  rmTree(tmp);
  mkdir(tmp.c_str(), 0755);
  mkdir((tmp + "/fx").c_str(), 0755);
  std::vector<uint8_t> buf(1 << 18);
  uint32_t lastDraw = 0;
  for (uint32_t i = 0; i < idx.n; i++) {
    const PakEntry &e = idx.e[i];
    std::string rel(e.name, e.len);
    if (rel.find("..") != std::string::npos || rel[0] == '/') continue;
    size_t slash = rel.rfind('/');
    if (slash != std::string::npos) mkdir((tmp + "/" + rel.substr(0, slash)).c_str(), 0755);
    FILE *out = fopen((tmp + "/" + rel).c_str(), "wb");
    if (!out) return "저장 공간에 쓸 수 없어요";
    uint32_t left = e.size, at = e.off;
    while (left) {
      uint32_t n = left < buf.size() ? left : (uint32_t)buf.size();
      if (!src.read(buf.data(), n, at)) { fclose(out); return "읽다가 멈췄어요"; }
      pakCrypt(aes, hdr.salt, at, buf.data(), n);
      if (fwrite(buf.data(), 1, n, out) != n) { fclose(out); return "저장 공간이 부족해요"; }
      left -= n; at += n; done += n;
      uint32_t now = millis();
      if (now - lastDraw > 150) {
        lastDraw = now;
        char l2[64];
        snprintf(l2, sizeof(l2), "%u / %u", (unsigned)(i + 1), (unsigned)idx.n);
        progressScreen("mons.pak 푸는 중...", l2, (uint32_t)(done >> 10), (uint32_t)(total >> 10));
      }
    }
    fclose(out);
  }
  // un paquete solo con musica (music.pak: sin thumbs) se AÑADE a lo que ya hay; uno completo lo sustituye
  struct stat st;
  bool full = stat((tmp + "/thumbs.bin").c_str(), &st) == 0 || stat((tmp + "/thumbs2.bin").c_str(), &st) == 0;
  if (!full && stat(fin.c_str(), &st) == 0) {
    gImportMerged = true;
    for (uint32_t i = 0; i < idx.n; i++) {
      std::string rel(idx.e[i].name, idx.e[i].len);
      if (rel.find("..") != std::string::npos || rel[0] == '/') continue;
      size_t slash = rel.rfind('/');
      if (slash != std::string::npos) mkdir((fin + "/" + rel.substr(0, slash)).c_str(), 0755);
      if (rename((tmp + "/" + rel).c_str(), (fin + "/" + rel).c_str()) != 0) return "파일을 옮길 수 없어요";
    }
    rmTree(tmp);
    return "";
  }
  gImportMerged = false;
  // los dibujos HD (hd.pak) se quedan aunque se cambie el mons.pak
  if (stat((fin + "/hd").c_str(), &st) == 0 && stat((tmp + "/hd").c_str(), &st) != 0) rename((fin + "/hd").c_str(), (tmp + "/hd").c_str());
  rmTree(fin);
  if (rename(tmp.c_str(), fin.c_str()) != 0) return "폴더를 바꿀 수 없어요";
  return "";
}

static void runImport(const std::vector<int> &fds) {
  progressScreen("mons.pak 푸는 중...", "", 0, 1);
  std::string err = !fds.empty() ? importPak(fds) : "파일이 없어요";
  for (int f : fds) close(f);
  if (err.empty()) {
    progressScreen(gImportMerged ? "추가 완료! (음악, HD 그림 등)" : "완료! 그림과 소리를 불러와요", "", 1, 1);
    sdDirty = true;
    monFor = -1;
    audioScanBgm();
    audioLoadMusic();
  } else {
    rmTree(gSdDir + "/mons_new");
    progressScreen("mons.pak을 풀지 못했어요", err.c_str(), 0, 0);
  }
  usleep(1500 * 1000);
}

static bool haveMons() {
  struct stat st;
  return stat((gSdDir + "/mons").c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

