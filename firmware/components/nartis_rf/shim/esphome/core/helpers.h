// Firmware port shim: ESPHome helpers used by the component.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "esp_mac.h"
namespace esphome {
inline uint32_t fnv1_hash(const std::string &str) {
  uint32_t hash = 2166136261UL;
  for (char c : str) { hash *= 16777619UL; hash ^= static_cast<uint8_t>(c); }
  return hash;
}
inline void get_mac_address_raw(uint8_t *mac) { esp_efuse_mac_get_default(mac); }
inline std::string format_hex_pretty(const uint8_t *data, size_t length) {
  std::string s;
  char b[4];
  for (size_t i = 0; i < length; i++) { snprintf(b, sizeof(b), i ? ".%02X" : "%02X", data[i]); s += b; }
  if (length > 4) s += " (" + std::to_string(length) + ")";
  return s;
}
inline std::string format_hex_pretty(const std::vector<uint8_t> &d) { return format_hex_pretty(d.data(), d.size()); }
inline std::string format_hex(const uint8_t *data, size_t length) {
  std::string s;
  char b[3];
  for (size_t i = 0; i < length; i++) { snprintf(b, sizeof(b), "%02x", data[i]); s += b; }
  return s;
}
}  // namespace esphome
