#include "settings.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

#define NS "nartis"
static settings_t s;

static void load_str(nvs_handle_t h, const char* k, char* dst, size_t n, const char* def) {
    size_t len = n;
    if(nvs_get_str(h, k, dst, &len) != ESP_OK) {
        strncpy(dst, def, n - 1);
        dst[n - 1] = 0;
    }
}

void settings_init(void) {
    memset(&s, 0, sizeof(s));
    nvs_handle_t h;
    if(nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        load_str(h, "ssid", s.wifi_ssid, sizeof(s.wifi_ssid), "");
        load_str(h, "pass", s.wifi_pass, sizeof(s.wifi_pass), "");
        load_str(h, "serial", s.meter_serial, sizeof(s.meter_serial), "");
        load_str(h, "channel", s.channel, sizeof(s.channel), "auto");
        if(nvs_get_u32(h, "interval", &s.interval) != ESP_OK) s.interval = 600;
        nvs_close(h);
    } else {
        strcpy(s.channel, "auto");
        s.interval = 600;
    }
}

const settings_t* settings_get(void) { return &s; }

bool settings_has_wifi(void) { return s.wifi_ssid[0] != 0; }

void settings_set_wifi(const char* ssid, const char* pass) {
    strncpy(s.wifi_ssid, ssid ? ssid : "", sizeof(s.wifi_ssid) - 1);
    strncpy(s.wifi_pass, pass ? pass : "", sizeof(s.wifi_pass) - 1);
    s.wifi_ssid[sizeof(s.wifi_ssid) - 1] = 0;
    s.wifi_pass[sizeof(s.wifi_pass) - 1] = 0;
    nvs_handle_t h;
    if(nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", s.wifi_ssid);
        nvs_set_str(h, "pass", s.wifi_pass);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_set_config(const char* serial, const char* channel, uint32_t interval) {
    if(serial) { strncpy(s.meter_serial, serial, sizeof(s.meter_serial) - 1); s.meter_serial[sizeof(s.meter_serial) - 1] = 0; }
    if(channel) { strncpy(s.channel, channel, sizeof(s.channel) - 1); s.channel[sizeof(s.channel) - 1] = 0; }
    if(interval) s.interval = interval;
    nvs_handle_t h;
    if(nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "serial", s.meter_serial);
        nvs_set_str(h, "channel", s.channel);
        nvs_set_u32(h, "interval", s.interval);
        nvs_commit(h);
        nvs_close(h);
    }
}
