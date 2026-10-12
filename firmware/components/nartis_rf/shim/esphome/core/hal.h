// Firmware port shim: ESPHome HAL timing helpers.
#pragma once
#include <cstdint>
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace esphome {
inline uint32_t millis() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
inline uint32_t micros() { return static_cast<uint32_t>(esp_timer_get_time()); }
inline void delay(uint32_t ms) { vTaskDelay(ms ? pdMS_TO_TICKS(ms) : 1); }
inline void delayMicroseconds(uint32_t us) { esp_rom_delay_us(us); }
inline void yield() { taskYIELD(); }
}  // namespace esphome
