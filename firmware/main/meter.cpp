/*
 * meter.cpp - glue between the Nartis RF stack (components/nartis_rf) and the
 * web/API layer. Runs the component on its own task, maps published OBIS
 * values into a snapshot the HTTP handlers read.
 */
#include "meter.h"
#include <cstring>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "settings.h"
#include "nartis_rf_meter.h"

using namespace esphome;
using esphome::nartis_rf_meter::NartisRfMeterComponent;
using esphome::nartis_rf_meter::ObisCode;

static const char* TAG = "meter";

// CC1101 wiring on the ESP32-C3 board.
static constexpr int PIN_SCK = 4;
static constexpr int PIN_MISO = 5;   // component name: pin_sdio
static constexpr int PIN_MOSI = 6;   // component name: pin_fcsb
static constexpr int PIN_CSN = 7;    // component name: pin_csb
static constexpr int PIN_GDO0 = 3;   // component name: pin_gpio3

// The meter sends no scaler/unit, so raw values are scaled here
// (factors from the upstream component's reference configuration).
// dtype: DLMS type the meter answers with (0x05 int32, 0x06 uint32, 0x12 uint16).
// Used to reject a reply that belongs to another batch (see nartis_rf_meter.cpp).
struct Def { const char* label; const char* obis; const char* unit; float mult; int decimals; uint8_t dtype; };
static const Def DEFS[] = {
    {"Мощность",       "1.0.1.7.0.255",  "Вт",    1.0f,   0, 0x05},
    {"Напряжение L1",  "1.0.32.7.0.255", "В",     0.1f,   1, 0x12},
    {"Напряжение L2",  "1.0.52.7.0.255", "В",     0.1f,   1, 0x12},
    {"Напряжение L3",  "1.0.72.7.0.255", "В",     0.1f,   1, 0x12},
    {"Ток L1",         "1.0.31.7.0.255", "А",     0.001f, 2, 0x06},
    {"Ток L2",         "1.0.51.7.0.255", "А",     0.001f, 2, 0x06},
    {"Ток L3",         "1.0.71.7.0.255", "А",     0.001f, 2, 0x06},
    {"Энергия",        "1.0.1.8.0.255",  "кВт·ч", 0.001f, 1, 0x06},   // meter reports Wh
    {"Тариф 1",        "1.0.1.8.1.255",  "кВт·ч", 0.001f, 1, 0x06},
    {"Тариф 2",        "1.0.1.8.2.255",  "кВт·ч", 0.001f, 1, 0x06},
    {"Частота",        "1.0.14.7.0.255", "Гц",    0.01f,  2, 0x12},
};
static constexpr int NDEFS = sizeof(DEFS) / sizeof(DEFS[0]);

static meter_state_t g_state;
static SemaphoreHandle_t g_lock;

static uint32_t uptime_s() { return static_cast<uint32_t>(esp_timer_get_time() / 1000000); }

class ValueSensor : public sensor::Sensor {
 public:
  ValueSensor(int idx, float mult) : idx_(idx), mult_(mult) { set_name(DEFS[idx].label); }
  void publish_state(float v) override {
    sensor::Sensor::publish_state(v);
    xSemaphoreTake(g_lock, portMAX_DELAY);
    meter_value_t& mv = g_state.values[idx_];
    mv.value = static_cast<double>(v) * mult_;
    mv.valid = true;
    mv.stale = false;
    g_state.last_update_s = uptime_s();
    xSemaphoreGive(g_lock);
    ESP_LOGI(TAG, "%s = %.3f %s", DEFS[idx_].label, static_cast<double>(v) * mult_, DEFS[idx_].unit);
  }
 private:
  int idx_;
  float mult_;
};

static ObisCode parse_obis(const char* s) {
  ObisCode o{};
  char* end = nullptr;
  for (int i = 0; i < 6; i++) {
    o.bytes[i] = static_cast<uint8_t>(strtoul(s, &end, 10));
    if (*end == '.') s = end + 1;
  }
  return o;
}

static void meter_task(void* arg) {
  (void) arg;
  const settings_t* cfg = settings_get();
  if (strlen(cfg->meter_serial) != 12) {
    ESP_LOGW(TAG, "meter serial not set (need 12 digits), radio idle");
    vTaskDelete(nullptr);
    return;
  }

  auto* c = new NartisRfMeterComponent();
  c->set_pin_sdio(PIN_MISO);
  c->set_pin_sclk(PIN_SCK);
  c->set_pin_csb(PIN_CSN);
  c->set_pin_fcsb(PIN_MOSI);
  c->set_pin_gpio3(PIN_GDO0);
  c->set_meter_serial(cfg->meter_serial);
  c->set_use_alternative_channels(true);
  if (strcmp(cfg->channel, "auto") != 0) c->set_fix_channel(static_cast<uint8_t>(atoi(cfg->channel)));
  c->set_batch_size(5);
  c->set_rx_timeout_ms(3000);
  c->set_rx_reply_timeout_ms(900);
  c->set_skip_fin_beacons(false);
  const uint32_t interval_ms = (cfg->interval ? cfg->interval : 600) * 1000;
  c->set_update_interval(interval_ms);
  for (int i = 0; i < NDEFS; i++) {
    c->register_sensor(new ValueSensor(i, DEFS[i].mult), parse_obis(DEFS[i].obis), 3, 2);
    c->set_sensor_expected_dtype(i, DEFS[i].dtype);
  }

  ESP_LOGI(TAG, "starting radio, meter %s, channel %s, interval %lus",
           cfg->meter_serial, cfg->channel, (unsigned long) (interval_ms / 1000));
  c->setup();

  uint32_t next_update = esphome::millis() + 10000;   // first poll shortly after boot
  for (;;) {
    c->loop();
    c->run_pending_();
    if (static_cast<int32_t>(esphome::millis() - next_update) >= 0) {
      next_update = esphome::millis() + interval_ms;
      c->update();
    }
    const bool paired = c->is_paired();
    if (paired != g_state.paired) {
      xSemaphoreTake(g_lock, portMAX_DELAY);
      g_state.paired = paired;
      xSemaphoreGive(g_lock);
      ESP_LOGI(TAG, "paired: %s", paired ? "yes" : "no");
    }
    vTaskDelay(1);
  }
}

extern "C" void meter_start(void) {
  g_lock = xSemaphoreCreateMutex();
  memset(&g_state, 0, sizeof(g_state));
  g_state.count = NDEFS;
  for (int i = 0; i < NDEFS; i++) {
    meter_value_t& v = g_state.values[i];
    strncpy(v.label, DEFS[i].label, sizeof(v.label) - 1);
    strncpy(v.obis, DEFS[i].obis, sizeof(v.obis) - 1);
    strncpy(v.unit, DEFS[i].unit, sizeof(v.unit) - 1);
    v.decimals = DEFS[i].decimals;
    v.stale = true;
  }
  // Radio stack logs at INFO; set these to ESP_LOG_DEBUG to trace pairing and DLMS frames.
  esp_log_level_set("nartis_rf_meter", ESP_LOG_INFO);
  esp_log_level_set("rf_data", ESP_LOG_INFO);
  esp_log_level_set("dlms_client", ESP_LOG_INFO);
  esp_log_level_set("nartis_cc1101", ESP_LOG_INFO);
  xTaskCreate(meter_task, "meter", 12288, nullptr, 4, nullptr);
}

extern "C" void meter_snapshot(meter_state_t* out) {
  if (!g_lock) { memset(out, 0, sizeof(*out)); return; }
  xSemaphoreTake(g_lock, portMAX_DELAY);
  *out = g_state;
  xSemaphoreGive(g_lock);
}
