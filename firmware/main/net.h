#pragma once
#include <stdbool.h>

typedef enum { NET_MODE_AP, NET_MODE_STA } net_mode_t;

void net_start(void);
net_mode_t net_mode(void);
bool net_sta_connected(void);
bool net_ap_active(void);         // setup AP (nartis-setup) is up
void net_get_ip(char* buf, int n);
int net_rssi(void);              // STA RSSI dBm, 0 if N/A
int net_scan(char json_out[], int cap);  // fills JSON {"networks":[...]}, returns len
