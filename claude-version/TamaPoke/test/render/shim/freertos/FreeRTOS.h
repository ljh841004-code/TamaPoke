#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
#define portMAX_DELAY 0xFFFFFFFFu
#define pdTRUE 1
#define pdPASS 1
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) (x)
#endif
