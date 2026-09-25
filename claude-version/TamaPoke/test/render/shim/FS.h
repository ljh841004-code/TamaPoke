#pragma once
#include <Arduino.h>
#define FILE_READ "rb"
#define FILE_WRITE "wb"
// File sobre stdio (lee de la carpeta de la SD simulada)
class File {
public:
  FILE *f = nullptr;
  File() {}
  explicit File(FILE *x) : f(x) {}
  explicit operator bool() const { return f != nullptr; }
  size_t read(uint8_t *b, size_t n) { return f ? fread(b, 1, n, f) : 0; }
  size_t write(const uint8_t *b, size_t n) { return f ? fwrite(b, 1, n, f) : 0; }
  bool seek(uint32_t p) { return f && fseek(f, p, SEEK_SET) == 0; }
  size_t size() { if (!f) return 0; long c = ftell(f); fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, c, SEEK_SET); return s; }
  void close() { if (f) fclose(f); f = nullptr; }
  const char *name() { return ""; }
  File openNextFile() { return File(); }
};
