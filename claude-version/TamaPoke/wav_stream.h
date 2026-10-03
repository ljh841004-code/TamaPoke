#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Reader must provide read(uint8_t*, size_t), seek(uint32_t), size(), close(),
// and bool conversion. No allocation proportional to the length of the song.
// ko11.31.4: RING = tamano del anillo (el firmware usa 32 KiB = 1 s; se pide a malloc al abrir,
// que en la placa lo pone en la PSRAM)
template<class Reader, size_t RING = 8192> class WavStream {
  Reader file;
  uint32_t dataStart = 0, dataBytes = 0, played = 0;
  // ko11.2: anillo de lectura adelantada. Antes se leia de golpe al vaciarse
  // (8 KiB de una vez justo cuando no quedaba nada): si en ese momento la tarea
  // se retrasaba, el I2S (90 ms de colchon) se quedaba sin datos y la musica
  // daba tirones. Ahora se rellena a trozos de 2 KiB en cuanto hay hueco, asi
  // que el anillo casi siempre esta lleno (~256 ms de margen).
  static constexpr size_t CHUNK = 2048;
  uint8_t *cache = nullptr;
  size_t head = 0, count = 0;  // lectura (bytes) y bytes validos del anillo
  uint32_t fillPos = 0;        // siguiente byte del chunk data a leer del fichero
public:
  WavStream() = default;
  WavStream(const WavStream &) = delete;
  WavStream &operator=(const WavStream &) = delete;
  ~WavStream() { free(cache); }
  uint32_t loops = 0;  // ko11: vueltas completas desde open()
private:
  static uint16_t u16(const uint8_t *p) { return p[0] | uint16_t(p[1]) << 8; }
  static uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  }
  // rellena el anillo mientras quede un trozo libre (o este vacio). Un fallo
  // de lectura cierra: nunca repetir audio corrupto
  void fill() {
    while (valid() && (count == 0 || RING - count >= CHUNK)) {
      if (fillPos == dataBytes) {
        fillPos = 0;
        if (!file.seek(dataStart)) { close(); return; }
      }
      size_t tail = (head + count) % RING;
      size_t want = RING - count;
      if (want > RING - tail) want = RING - tail;  // hueco contiguo
      if (want > CHUNK) want = CHUNK;
      if (want > dataBytes - fillPos) want = dataBytes - fillPos;
      size_t got = file.read(cache + tail, want);
      if (got != want) { close(); return; }
      count += want;
      fillPos += want;
    }
  }
public:
  void close() { file.close(); dataStart = dataBytes = played = fillPos = 0; head = count = 0; }
  bool valid() const { return dataBytes != 0; }
  size_t buffered() const { return count; }  // ko11.31.3: bytes listos en el anillo
  static constexpr size_t ringSize() { return RING; }
  uint32_t position() const { return played; }
  uint32_t lengthBytes() const { return dataBytes; }  // ko10.4: duracion = bytes / 32000 s
  bool open(Reader input, uint32_t resume = 0) {
    close(); file = input; loops = 0;
    if (!cache) cache = (uint8_t *)malloc(RING);
    if (!cache) { close(); return false; }
    uint8_t h[16];
    if (!file || file.size() < 12 || file.read(h, 12) != 12 ||
        memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { close(); return false; }
    uint32_t riff = u32(h + 4);
    if (riff < 4 || riff > file.size() - 8) { close(); return false; }
    uint32_t end = riff + 8, pos = 12;
    bool format = false;
    while (pos <= end && end - pos >= 8) {
      if (!file.seek(pos) || file.read(h, 8) != 8) break;
      uint32_t bytes = u32(h + 4), start = pos + 8;
      if (bytes > end - start) break;
      if (!memcmp(h, "fmt ", 4)) {
        if (bytes < 16 || file.read(h, 16) != 16 || u16(h) != 1 ||
            u16(h + 2) != 1 || u32(h + 4) != 16000 ||
            u32(h + 8) != 32000 || u16(h + 12) != 2 || u16(h + 14) != 16) break;
        format = true;
      } else if (!memcmp(h, "data", 4)) {
        if (!format || !bytes || (bytes & 1)) break;
        dataStart = start; dataBytes = bytes;
        played = resume < bytes ? resume & ~1u : 0;
        fillPos = played;
        if (file.seek(dataStart + played)) return true;
        break;
      }
      if ((bytes & 1) && bytes == end - start) break;
      pos = start + bytes + (bytes & 1);
    }
    close(); return false;
  }
  // Exactly loops the data chunk, excluding metadata, including partial tails.
  // A short read is an I/O failure: stop instead of looping corrupt audio.
  // allowIO=false: only the read-ahead (another task owns the SD card).
  size_t read(int16_t *out, size_t samples, bool allowIO = true) {
    size_t done = 0;
    if (allowIO) fill();
    while (done < samples && valid()) {
      if (count < 2) {
        if (!allowIO) break;
        fill();
        if (count < 2) break;
      }
      if (played == dataBytes) {  // la vuelta cuenta al consumir, no al leer
        played = 0;
        loops++;  // ko11: una vuelta completa (para cambiar de cancion al acabar)
      }
      out[done++] = (int16_t)u16(cache + head);
      head = (head + 2) % RING; count -= 2; played += 2;
    }
    if (allowIO) fill();  // ko11.2: dejar el anillo lleno para el siguiente bloque
    return done;
  }
};
