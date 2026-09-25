#include "sdupdate.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <Update.h>
#include "sd_lock.h"
#include "sdmon.h"
#include "audio.h"

static uint8_t head[UPD_HEAD_LEN];

UpdCheck sdUpdateCheck(uint32_t *size) {
  if (size) *size = 0;
  if (!sdReady) return UPD_NONE;
  SdCardLock lock;
  if (!lock) return UPD_NONE;
  File f = SD_MMC.open("/update.bin", FILE_READ);
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
  File f = SD_MMC.open("/update.bin", FILE_READ);
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
  SD_MMC.remove("/update_done.bin");
  SD_MMC.rename("/update.bin", "/update_done.bin");
  Serial.println("UPD ok");
  return true;
}
