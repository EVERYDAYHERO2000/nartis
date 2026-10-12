// Firmware port shim: subset of ESPHome ESPTime.
#pragma once
#include <cstdint>
#include <ctime>
namespace esphome {
struct ESPTime {
  uint8_t second{0}, minute{0}, hour{0};
  uint8_t day_of_week{0};   // 1 = Sunday ... 7 = Saturday (ESPHome convention)
  uint8_t day_of_month{0}, month{0};
  uint16_t day_of_year{0}, year{0};
  time_t timestamp{0};
  bool is_valid() const { return year >= 2019 && month >= 1; }
  static ESPTime from_epoch_utc(time_t epoch) {
    struct tm t;
    gmtime_r(&epoch, &t);
    ESPTime r;
    r.second = t.tm_sec; r.minute = t.tm_min; r.hour = t.tm_hour;
    r.day_of_week = t.tm_wday + 1; r.day_of_month = t.tm_mday; r.month = t.tm_mon + 1;
    r.day_of_year = t.tm_yday + 1; r.year = t.tm_year + 1900; r.timestamp = epoch;
    return r;
  }
};
}  // namespace esphome
