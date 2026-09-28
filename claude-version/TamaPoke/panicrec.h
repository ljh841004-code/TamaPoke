#pragma once
// ko11.9.3: direcciones del codigo en el ultimo panic (ver panicrec.cpp)
#include <stdint.h>
#define PANIC_PCS 6
#define PANIC_MAGIC 0x50414E43u
void panicRecBegin();  // en setup(), lo antes posible
// true si el reinicio anterior fue un panic grabado (y lo borra)
bool panicTake(uint32_t *pcs, uint8_t *n, uint32_t *exc);
