// ko12.10.17: frames lisos del combate (도트): la cache (SmAA), los buffers de trabajo y los encargos a la
// tarea de fondo. Aqui y no en el .ino: el preprocesador de Arduino pone los prototipos de las funciones antes
// que los struct del sketch
#pragma once
#include <stdint.h>

// un frame ya agrandado y suavizado, listo para copiar al framebuffer
struct SmAA {
  const uint8_t *src = nullptr;
  const uint16_t *pal = nullptr;
  int s4 = 0, W = 0, H = 0;
  int FW = 0, FH = 0, cx = 0, cy = 0;  // ko12.10.6: tamano entero y recorte (solo lo visible del frame)
  uint16_t *col = nullptr;  // W*H colores y detras W*H alfas (0..4), en un solo bloque
  uint8_t *a = nullptr;
  uint32_t used = 0;
  void release();
};
// buffers de trabajo (uno por hilo: el bucle principal y la tarea de fondo)
struct SmScratch { uint8_t *A = nullptr, *B = nullptr, *C = nullptr; uint16_t *tmp = nullptr; };
// encargo a la tarea de fondo: lleva COPIA del frame y de la paleta
struct SmJob {
  const uint8_t *key; const uint16_t *palKey;
  uint32_t gen; int w, h, s4, res;
  uint16_t pal[256];
  SmAA out;
  uint8_t frame[1];  // w*h bytes (se pide memoria de mas)
};
