// Firmware port shim: NVS-backed ESPHome preferences.
#include <cstdio>
#include "esphome/core/preferences.h"
#include "nvs.h"

namespace esphome {

static ESPPreferences s_prefs;
ESPPreferences *global_preferences = &s_prefs;

static const char *NS = "esph_pref";

bool ESPPreferenceObject::save_(const void *data, size_t len) {
  if (!valid_) return false;
  char key[16];
  snprintf(key, sizeof(key), "p%08lx", (unsigned long) key_);
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_set_blob(h, key, data, len) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

bool ESPPreferenceObject::load_(void *data, size_t len) {
  if (!valid_) return false;
  char key[16];
  snprintf(key, sizeof(key), "p%08lx", (unsigned long) key_);
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
  size_t got = len;
  bool ok = nvs_get_blob(h, key, data, &got) == ESP_OK && got == len;
  nvs_close(h);
  return ok;
}

}  // namespace esphome
