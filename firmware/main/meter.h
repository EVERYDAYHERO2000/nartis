#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define METER_MAX_VALUES 16

typedef struct {
    char label[40];
    char obis[20];
    char unit[16];
    int decimals;
    double value;
    bool valid;
    bool stale;
} meter_value_t;

typedef struct {
    bool paired;
    uint32_t last_update_s;    // uptime seconds at last value, 0 = never
    int count;
    meter_value_t values[METER_MAX_VALUES];
} meter_state_t;

// Starts the CC1101 radio task (pairing + periodic DLMS poll).
void meter_start(void);

// Thread-safe snapshot of the current readings.
void meter_snapshot(meter_state_t* out);

#ifdef __cplusplus
}
#endif
