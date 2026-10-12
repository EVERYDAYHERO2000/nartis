// Firmware port shim: ESPHome logging -> ESP-IDF logging (same macro names).
#pragma once
#include "esp_log.h"
#define ESP_LOGCONFIG(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#define ESP_LOGVV(tag, ...) ESP_LOGV(tag, __VA_ARGS__)

// ESPHome flash-string logging helpers (plain C strings here).
namespace esphome {
using LogString = char;
}
#define LOG_STR(s) (s)
#define LOG_STR_ARG(s) (s)
#define LOG_STR_LITERAL(s) (s)
