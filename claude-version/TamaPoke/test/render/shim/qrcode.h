#pragma once
// test/render: la misma interfaz que el componente espressif/qrcode del core
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef const uint8_t *esp_qrcode_handle_t;
typedef struct {
  void (*display_func)(esp_qrcode_handle_t qrcode);
  int max_qrcode_version;
  int qrcode_ecc_level;
} esp_qrcode_config_t;
enum { ESP_QRCODE_ECC_LOW, ESP_QRCODE_ECC_MED, ESP_QRCODE_ECC_QUART, ESP_QRCODE_ECC_HIGH };
esp_err_t esp_qrcode_generate(esp_qrcode_config_t *cfg, const char *text);
int esp_qrcode_get_size(esp_qrcode_handle_t qrcode);
bool esp_qrcode_get_module(esp_qrcode_handle_t qrcode, int x, int y);
