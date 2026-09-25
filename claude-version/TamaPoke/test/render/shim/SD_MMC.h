#pragma once
#include "FS.h"
extern const char *gSdRoot;
struct SDMMCFS {
  void setPins(int, int, int) {}
  bool begin(const char *, bool, bool) { return true; }
  void end() {}
  uint64_t cardSize() { return 1ULL << 30; }
  uint64_t totalBytes() { return 1ULL << 30; }
  uint64_t usedBytes() { return 0; }
  bool mkdir(const char *) { return true; }
  bool exists(const char *) { return false; }
  bool remove(const char *) { return false; }
  bool exists(const String &) { return false; }
  bool remove(const String &) { return false; }
  File open(const String &p, const char *mode) { return open(p.c_str(), mode); }
  File open(const char *p, const char *mode = FILE_READ) {
    std::string full = std::string(gSdRoot) + p;
    return File(fopen(full.c_str(), mode));
  }
};
extern SDMMCFS SD_MMC;
