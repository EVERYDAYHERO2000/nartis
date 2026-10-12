// Firmware port shim: ESPHome preferences backed by an NVS blob per key.
#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome {
class ESPPreferenceObject {
 public:
  ESPPreferenceObject() = default;
  explicit ESPPreferenceObject(uint32_t key) : key_(key), valid_(true) {}
  template<typename T> bool save(const T *src) { return save_(src, sizeof(T)); }
  template<typename T> bool load(T *dst) { return load_(dst, sizeof(T)); }
 private:
  bool save_(const void *data, size_t len);
  bool load_(void *data, size_t len);
  uint32_t key_{0};
  bool valid_{false};
};
class ESPPreferences {
 public:
  template<typename T> ESPPreferenceObject make_preference(uint32_t key, bool in_flash = true) {
    (void) in_flash;
    return ESPPreferenceObject(key);
  }
  bool sync() { return true; }  // writes are committed immediately
};
extern ESPPreferences *global_preferences;
}  // namespace esphome
