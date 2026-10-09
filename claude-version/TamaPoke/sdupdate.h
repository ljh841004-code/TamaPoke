#pragma once
// fork KO (ko5): actualizar el firmware desde /update.bin de la SD.
//
// El .bin se escribe en la otra particion de app (app0/app1) con la libreria
// Update del core; solo si se escribe y se verifica entero se cambia la de
// arranque. Un fallo a medias deja el firmware de antes.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum UpdCheck : uint8_t {
  UPD_OK = 0,
  UPD_NONE,      // no hay /update.bin
  UPD_FULLIMG,   // es la imagen completa de 0x0 (bootloader + particiones), no la app
  UPD_BAD,       // no es una app de ESP32-S3 o el tamano no cuadra
};

// ko11.6: apps de 6 MB con la tabla nueva. Con la vieja (3 MB) Update.begin()
// rechaza una imagen que no quepa en la particion, asi que no hay peligro
#define UPD_MAX_SIZE (6UL * 1024 * 1024)
#define UPD_HEAD_LEN 0x8002               // para ver si hay tabla de particiones en 0x8000

// Clasifica un fichero por su cabecera (logica pura: test/test_box.cpp).
// head = los primeros bytes (hasta UPD_HEAD_LEN), size = tamano total.
static inline UpdCheck updClassify(const uint8_t *head, size_t headLen, uint32_t size) {
  if (size < 0x1000 || size > UPD_MAX_SIZE + UPD_HEAD_LEN || headLen < 16) return UPD_BAD;
  if (head[0] != 0xE9) return UPD_BAD;                       // magia de imagen ESP
  // imagen fusionada de 0x0: en 0x8000 empieza la tabla de particiones (AA 50)
  if (headLen >= UPD_HEAD_LEN && head[0x8000] == 0xAA && head[0x8001] == 0x50) return UPD_FULLIMG;
  if (size > UPD_MAX_SIZE) return UPD_BAD;
  if ((head[12] | head[13] << 8) != 9) return UPD_BAD;       // chip_id 9 = ESP32-S3
  return UPD_OK;
}

// ko12.8.4: el fichero de esptool para 0xe000 (8 KB de boot_app0 + la app) tambien vale para la
// SD: la app empieza 0x2000 mas alla. Antes se rechazaba como "no hay fichero" (y durante varias
// versiones el update.bin publicado era justo ese fichero)
#define UPD_E000_OFS 0x2000
static inline uint32_t updAppOffset(const uint8_t *head, size_t headLen) {
  if (headLen > UPD_E000_OFS + 16 && head[0] != 0xE9 && head[UPD_E000_OFS] == 0xE9) return UPD_E000_OFS;
  return 0;
}

// ko6.2: marca de version dentro del firmware ("TPVER:" + FW_VERSION). La
// pantalla de actualizacion la busca en update.bin para ensenar que version trae.
#define UPD_TAG "TPVER:"

// posicion de la marca en buf (-1 si no esta). Logica pura (tests).
static inline int updFindTag(const uint8_t *buf, size_t len) {
  const size_t tl = sizeof(UPD_TAG) - 1;
  for (size_t i = 0; i + tl <= len; i++)
    if (buf[i] == 'T' && !memcmp(buf + i, UPD_TAG, tl)) return (int)i;
  return -1;
}

// En la placa (sdupdate.cpp)
UpdCheck sdUpdateCheck(uint32_t *size);
// escribe y verifica; progress(done, total) se llama por bloque. true = listo
// para reiniciar (y /update.bin pasa a /update_done.bin)
bool sdUpdateRun(void (*progress)(uint32_t done, uint32_t total));
// version que trae el update.bin encontrado por sdUpdateCheck ("" si no se sabe)
bool sdUpdateFileVersion(char *out, size_t n);

// ko12.9.2: actualizar por WiFi (portal 192.168.4.1/fw). El fichero llega a trozos: se guardan los
// primeros UPD_HEAD_LEN bytes para clasificarlo igual que en la SD (app normal, fichero de 0xe000 o
// imagen completa de 0x0, que se rechaza) y solo entonces se empieza a escribir. Logica pura con
// "sink" (Update en la placa, un buffer en test/test_box.cpp).
struct UpdSink {
  bool (*begin)(void *ctx);                                // empezar a escribir la app
  bool (*write)(void *ctx, const uint8_t *d, size_t n);    // un trozo de la app
  void *ctx;
};
struct UpdStream {
  uint8_t *head = nullptr;  // UPD_HEAD_LEN bytes (lo pone quien llama)
  size_t headN = 0;
  bool decided = false;
  UpdCheck err = UPD_OK;    // UPD_NONE = vacio; UPD_FULLIMG / UPD_BAD = rechazado; UPD_OK = bien
  uint32_t skip = 0;        // bytes del principio que no son la app (0x2000 en el de 0xe000)
  uint32_t total = 0;       // bytes recibidos
  uint32_t written = 0;     // bytes de app escritos
  bool failed = false;
  UpdSink sink{};

  void reset(uint8_t *buf, const UpdSink &s) { *this = UpdStream(); head = buf; sink = s; }
  bool feed(const uint8_t *d, size_t n) {
    if (failed) return false;
    total += (uint32_t)n;
    if (!decided) {
      size_t k = UPD_HEAD_LEN - headN < n ? UPD_HEAD_LEN - headN : n;
      memcpy(head + headN, d, k);
      headN += k; d += k; n -= k;
      if (headN < UPD_HEAD_LEN) return true;
      if (!decide(false)) return false;
    }
    return put(d, n);
  }
  bool finish() {  // true = la app entera se escribio (falta Update.end)
    if (failed) return false;
    if (!decided && !decide(true)) return false;
    if (written == 0) { err = UPD_NONE; failed = true; return false; }
    if (written > UPD_MAX_SIZE) { err = UPD_BAD; failed = true; return false; }
    return true;
  }

 private:
  bool decide(bool final) {
    decided = true;
    if (headN == 0) { err = UPD_NONE; failed = true; return false; }
    skip = updAppOffset(head, headN);
    if (skip >= headN) skip = 0;
    // el tamano de verdad aun no se sabe: 1 MB solo para pasar el limite; el final se mira en finish()
    uint32_t sz = final ? (uint32_t)(headN - skip) : 0x100000u;
    err = updClassify(head + skip, headN - skip, sz);
    if (err != UPD_OK) { failed = true; return false; }
    if (!sink.begin(sink.ctx)) { err = UPD_BAD; failed = true; return false; }
    return put(head + skip, headN - skip);
  }
  bool put(const uint8_t *d, size_t n) {
    if (!n) return true;
    if (written + n > UPD_MAX_SIZE || !sink.write(sink.ctx, d, n)) { failed = true; return false; }
    written += (uint32_t)n;
    return true;
  }
};
