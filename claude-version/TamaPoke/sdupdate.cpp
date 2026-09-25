#include "sdupdate.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <Update.h>
#include "sd_lock.h"
#include "sdmon.h"
#include "audio.h"

static uint8_t head[UPD_HEAD_LEN];
// ko5.1: tambien en /mons/: el instalador web (PUT por serie) solo escribe ahi
static const char *const UPD_PATHS[2] = { "/update.bin", "/mons/update.bin" };
static const char *updPath = UPD_PATHS[0];

UpdCheck sdUpdateCheck(uint32_t *size) {
  if (size) *size = 0;
  if (!sdReady) return UPD_NONE;
  SdCardLock lock;
  if (!lock) return UPD_NONE;
  File f;
  for (const char *p : UPD_PATHS) {
    f = SD_MMC.open(p, FILE_READ);
    if (f) { updPath = p; break; }
  }
  if (!f) return UPD_NONE;
  uint32_t sz = f.size();
  size_t n = f.read(head, sizeof(head));
  f.close();
  if (size) *size = sz;
  return updClassify(head, n, sz);
}

bool sdUpdateRun(void (*progress)(uint32_t done, uint32_t total)) {
  uint32_t size = 0;
  if (sdUpdateCheck(&size) != UPD_OK) return false;
  // la musica suelta la SD, como en el PUT por serie
  if (!audioPauseForUpload()) { audioResumeAfterUpload(); return false; }
  struct ResumeAudio { ~ResumeAudio() { audioResumeAfterUpload(); } } resumeAudio;
  SdCardLock lock;
  if (!lock) return false;
  File f = SD_MMC.open(updPath, FILE_READ);
  if (!f) return false;
  if (!Update.begin(size, U_FLASH)) {
    Serial.printf("UPD begin: %s\n", Update.errorString());
    f.close();
    return false;
  }
  static uint8_t buf[4096];
  uint32_t done = 0;
  bool ok = true;
  while (done < size) {
    size_t want = size - done > sizeof(buf) ? sizeof(buf) : size - done;
    size_t n = f.read(buf, want);
    if (n != want || Update.write(buf, n) != n) { ok = false; break; }
    done += n;
    if (progress) progress(done, size);
  }
  f.close();
  if (!ok) {
    Serial.printf("UPD write: %s\n", Update.errorString());
    Update.abort();
    return false;
  }
  if (!Update.end(true)) {  // verifica la imagen y la marca como la de arranque
    Serial.printf("UPD end: %s\n", Update.errorString());
    return false;
  }
  // que no se vuelva a aplicar sin querer
  char used[32];  // /update_done.bin o /mons/update_done.bin (misma carpeta)
  snprintf(used, sizeof(used), "%.*supdate_done.bin", (int)(strrchr(updPath, '/') - updPath + 1), updPath);
  SD_MMC.remove(used);
  SD_MMC.rename(updPath, used);
  Serial.println("UPD ok");
  return true;
}

bool sdUpdateFileVersion(char *out, size_t n) {
  out[0] = 0;
  if (!sdReady || n < 2) return false;
  SdCardLock lock;
  if (!lock) return false;
  File f = SD_MMC.open(updPath, FILE_READ);
  if (!f) return false;
  // por bloques con solape, para no perder una marca partida entre dos
  static uint8_t buf[4096 + 32];
  size_t keep = 0;
  bool found = false;
  while (!found) {
    size_t got = f.read(buf + keep, 4096);
    if (!got) break;
    size_t len = keep + got;
    // puede haber varias: la propia cadena "TPVER:" que usa esta funcion para
    // buscar tambien esta en el binario, sin version detras. Vale la que sigue
    // con un digito.
    size_t from = 0;
    int at;
    while ((at = updFindTag(buf + from, len - from)) >= 0) {
      size_t p = from + at + sizeof(UPD_TAG) - 1;
      if (p + n > len && len < sizeof(buf)) {  // la version sigue en el bloque siguiente
        len += f.read(buf + len, sizeof(buf) - len);
      }
      if (p < len && buf[p] >= '0' && buf[p] <= '9') {
        size_t j = 0;
        while (p < len && j < n - 1 && buf[p] >= 0x20 && buf[p] < 0x7F) out[j++] = buf[p++];
        out[j] = 0;
        found = j > 0;
        break;
      }
      from += at + 1;
    }
    if (found) break;
    keep = len < 32 ? len : 32;
    memmove(buf, buf + len - keep, keep);
  }
  f.close();
  return found;
}
