#pragma once
#include <stdint.h>

inline bool wildMusicActive(bool battleOpen, bool trainer, uint8_t outcome, bool screenOff) {
  return battleOpen && !trainer && !outcome && !screenOff;
}

// Preserve the wild-song cursor across results/trainer waves within one run.
// A fresh run gets a fresh session ID. Ordinary BGM always restarts on return.
class MusicRoute {
  uint32_t request = UINT32_MAX, reload = UINT32_MAX, wildPosition = 0;
public:
  bool changed(uint32_t next, uint32_t revision) const {
    return next != request || revision != reload;
  }
  uint32_t switchTo(uint32_t next, uint32_t revision, uint32_t position, bool valid) {
    if ((next >> 1) != (request >> 1)) wildPosition = 0;
    else if ((request & 1u) && valid) wildPosition = position;
    request = next; reload = revision;
    return next & 1u ? wildPosition : 0;
  }
};
