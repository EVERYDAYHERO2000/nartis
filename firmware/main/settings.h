#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char meter_serial[13];   // 12 digits + NUL
    char channel[5];         // "auto" | "0".."3"
    uint32_t interval;       // poll interval, seconds
} settings_t;

void settings_init(void);
const settings_t* settings_get(void);
void settings_set_wifi(const char* ssid, const char* pass);
void settings_set_config(const char* serial, const char* channel, uint32_t interval);
bool settings_has_wifi(void);

#ifdef __cplusplus
}
#endif
