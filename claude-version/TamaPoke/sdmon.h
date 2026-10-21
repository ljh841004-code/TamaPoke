#pragma once
#include <Arduino.h>
#include <FS.h>

// Sprite animado TPK1 (formato heredado, camino de respaldo). El proyecto usa
// PMD/TPK2 (PmdMon) para todo; esta ruta queda inactiva si no hay NNN.bin en la SD.
// Los datos indexados viven en PSRAM; la paleta es RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // factor de zoom entero al dibujar
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indices (0xFF = transparente)

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// acciones de los sprites PMD (formato TPK2)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  // ko10.8: extra (bloque EXT1 al final del archivo; los sprites viejos no las traen)
  PMD_ROTATE, PMD_CHARGE, PMD_SHOOT, PMD_LAYING,
  // ko11.15.1: combate mirandose (fila 3 = arriba-derecha, de espaldas; fila 7 = abajo-izquierda).
  // Van en el mismo bloque EXT1: un firmware viejo salta estos ids
  PMD_IDLE_UR, PMD_ATTACK_UR, PMD_HURT_UR, PMD_IDLE_DL, PMD_ATTACK_DL, PMD_HURT_DL,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // fila+1 del pixel mas bajo (anclar por los pies, no el lienzo)
  uint16_t ms[24];
  const uint8_t *data = nullptr;  // frames * w * h en el blob
  // ko12.10.11: TPK3 (los "3D"): cada frame comprimido por filas: [salto, n, n bytes]... hasta w.
  // offs = nf+1 desplazamientos (uint32, sin alinear) desde 'data'; nullptr = sin comprimir (TPK2)
  const uint8_t *offs = nullptr;
  const uint8_t *frameRle(uint8_t f) const {
    uint32_t o;
    memcpy(&o, offs + 4 * f, 4);
    return data + o;
  }
};

// sprite PMD multi-accion cargado de la SD a PSRAM
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  PmdAct acts[PMD_NACTS];

  // kind: 'p' = pNNN.bin (PMD, todas las acciones); 'r' = rNNN.bin (ko11.16: combate estilo juego)
  bool load(uint8_t dexNum, bool shiny = false, char kind = 'p');
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// ko12.10.14: carga en segundo plano (otra tarea lee la SD; la pantalla sigue). kinds = los ficheros a
// probar en orden ("fr": 3D y si no el original); got = el que se cargo (0 = ninguno), al terminar.
// Mientras, pmdPending(&dst) es true y dst esta vacio. dst.unload() (o un load normal) cancela lo pendiente.
// pmdAsyncPoll() en el bucle principal (coloca lo que ya llego).
void pmdLoadAsync(PmdMon &dst, uint8_t dexNum, bool shiny, const char *kinds, char *got = nullptr);
bool pmdPending(const PmdMon *dst);
uint8_t pmdAsyncPoll(void (*swapped)(PmdMon *) = nullptr);  // cuantos se colocaron; swapped: cada uno (ko12.10.17)

// miniaturas de la galeria (thumbs.bin entero en PSRAM)
struct SdThumbs {
  const char *path = "/mons/thumbs.bin";  // ko11.21: el mismo formato sirve para los retratos (story.bin)
  const char *alt = nullptr;  // ko12.9.23: se prueba antes (thumbs2.bin: recortadas, hasta 48 px)
  bool loaded = false;
  uint8_t *data = nullptr;
  uint16_t count = 0;
  uint32_t size = 0;  // bytes leidos: acota los offsets del fichero
  bool load();
  void unload();  // libera el blob: recargar tras recibir un thumbs.bin nuevo
  const uint8_t *get(int16_t dex) const;  // blob: w,h,palCount,pal[],idx[]
};
extern SdThumbs thumbs;
extern SdThumbs portraits;  // ko11.21: /mons/story.bin (retratos de la historia, id 1..N)

bool sdBegin();                 // monta la SD (SDMMC 1-bit), true si hay tarjeta
bool sdRemount();               // ko11.16.2: desmonta y vuelve a montar (en marcha)
bool sdSerialCommand(const String &line);  // PUT/LS por USB; true si la maneja
extern bool sdReady;
extern bool sdDirty;  // true tras recibir archivos: recargar sprite
// ko11.32: rutas que ya se buscaron y no estaban. En la FAT abrir un fichero que no existe
// recorre la carpeta entera (mons tiene ~1000 ficheros): asi solo se busca una vez.
// Se olvida al recibir ficheros (PUT) o al volver a montar la SD. Solo el bucle principal
bool sdMaybe(const char *path);        // false = ya se sabe que no esta
void sdMarkMissing(const char *path);
void sdForgetMissing();
File sdOpenKnown(const char *path);    // SD_MMC.open(path) salvo que se sepa que no esta (con el cerrojo ya cogido)
