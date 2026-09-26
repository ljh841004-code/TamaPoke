#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Reader must provide read(uint8_t*, size_t), seek(uint32_t), size(), close(),
// and bool conversion. No allocation proportional to the length of the song.
template<class Reader> class WavStream {
  Reader file;
  uint32_t dataStart = 0, dataBytes = 0, played = 0;
  uint8_t cache[8192];
  size_t at = 0, count = 0;
  static uint16_t u16(const uint8_t *p) { return p[0] | uint16_t(p[1]) << 8; }
  static uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  }
public:
  void close() { file.close(); dataStart = dataBytes = played = 0; at = count = 0; }
  bool valid() const { return dataBytes != 0; }
  uint32_t position() const { return played; }
  uint32_t lengthBytes() const { return dataBytes; }  // ko10.4: duracion = bytes / 32000 s
  bool open(Reader input, uint32_t resume = 0) {
    close(); file = input;
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
  size_t read(int16_t *out, size_t samples, bool allowIO = true) {
    size_t done = 0;
    while (done < samples && valid()) {
      if (at == count) {
        if (!allowIO) break; // consume read-ahead while another task owns SD
        if (played == dataBytes) {
          played = 0;
          if (!file.seek(dataStart)) { close(); break; }
        }
        size_t want = dataBytes - played;
        if (want > sizeof(cache)) want = sizeof(cache);
        count = file.read(cache, want); at = 0;
        if (count != want) { close(); break; }
      }
      out[done++] = (int16_t)u16(cache + at);
      at += 2; played += 2;
    }
    return done;
  }
};
