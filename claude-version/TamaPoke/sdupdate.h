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

#define UPD_MAX_SIZE (3UL * 1024 * 1024)  // particion de app de 3 MB
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
