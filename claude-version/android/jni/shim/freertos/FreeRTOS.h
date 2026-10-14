#pragma once
// Android: FreeRTOS minimo con hilos de verdad (el audio corre en su propio hilo, como en la placa)
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
#define portMAX_DELAY 0xFFFFFFFFu
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portTICK_PERIOD_MS 1
