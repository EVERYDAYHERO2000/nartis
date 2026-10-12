#include "net.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/sockets.h"

// ESP32-C3 Super Mini: at the default ~20 dBm the transmitted signal is
// distorted and access points cannot decode it (auth timeouts, invisible
// SoftAP). 8.5 dBm (34 quarter-dBm) is reliable on these boards.
#define TX_POWER_QDBM  34
#define AP_SSID        "nartis-setup"
#define FALLBACK_AFTER 10      // failed STA attempts before the setup AP opens
#define RETRY_PERIOD_S 30      // STA retry period while the setup AP is open

static const char* TAG = "net";
static net_mode_t g_mode = NET_MODE_AP;
static volatile bool g_ap_active;
static volatile int g_fail;
static EventGroupHandle_t g_evt;
#define BIT_CONNECTED BIT0

// ---- captive-portal DNS: answer every A query with the AP IP ----
static void dns_task(void* arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock < 0) { vTaskDelete(NULL); return; }
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(53),
                            .sin_addr.s_addr = htonl(INADDR_ANY)};
    if(bind(sock, (struct sockaddr*)&a, sizeof(a)) < 0) { close(sock); vTaskDelete(NULL); return; }
    uint8_t buf[256];
    for(;;) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&src, &sl);
        if(n < 12 || n + 16 > (int)sizeof(buf)) continue;
        if(!g_ap_active) continue;
        buf[2] |= 0x80;  // QR = response
        buf[3] |= 0x80;  // RA
        buf[6] = 0; buf[7] = 1;   // ANCOUNT = 1
        buf[8] = buf[9] = buf[10] = buf[11] = 0;
        uint8_t* p = buf + n;
        *p++ = 0xC0; *p++ = 0x0C;          // name pointer to question
        *p++ = 0x00; *p++ = 0x01;          // type A
        *p++ = 0x00; *p++ = 0x01;          // class IN
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3C;  // TTL 60
        *p++ = 0x00; *p++ = 0x04;          // RDLENGTH 4
        *p++ = 192; *p++ = 168; *p++ = 4; *p++ = 1;
        sendto(sock, buf, p - buf, 0, (struct sockaddr*)&src, sl);
    }
}

// Bring up the setup AP alongside STA (APSTA keeps scanning available).
static void open_setup_ap(void) {
    static bool dns_started;
    wifi_config_t ap = {0};
    strcpy((char*)ap.ap.ssid, AP_SSID);
    ap.ap.ssid_len = strlen(AP_SSID);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_set_max_tx_power(TX_POWER_QDBM);
    g_ap_active = true;
    if(!dns_started) {
        dns_started = true;
        xTaskCreate(dns_task, "dns", 3072, NULL, 5, NULL);
    }
    ESP_LOGI(TAG, "setup AP %s up, http://192.168.4.1", AP_SSID);
}

static void close_setup_ap(void) {
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_max_tx_power(TX_POWER_QDBM);
    g_ap_active = false;
    ESP_LOGI(TAG, "setup AP closed");
}

static void on_wifi(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if(base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if(g_mode == NET_MODE_STA) esp_wifi_connect();
    } else if(base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if(g_mode != NET_MODE_STA) return;
        wifi_event_sta_disconnected_t* d = data;
        xEventGroupClearBits(g_evt, BIT_CONNECTED);
        g_fail++;
        ESP_LOGW(TAG, "disconnected, reason %d (fail %d)", d->reason, g_fail);
        if(!g_ap_active) esp_wifi_connect();   // with the AP open, monitor retries slowly
    } else if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ev = data;
        g_fail = 0;
        xEventGroupSetBits(g_evt, BIT_CONNECTED);
        ESP_LOGI(TAG, "connected, ip " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

// Opens the setup AP when STA keeps failing, retries STA, closes the AP on success.
static void monitor_task(void* arg) {
    (void)arg;
    uint32_t t = 0;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        t++;
        bool conn = xEventGroupGetBits(g_evt) & BIT_CONNECTED;
        if(!conn && !g_ap_active && g_fail >= FALLBACK_AFTER) {
            ESP_LOGW(TAG, "cannot join %s, opening setup AP", settings_get()->wifi_ssid);
            open_setup_ap();
        } else if(g_ap_active && !conn && t % RETRY_PERIOD_S == 0) {
            esp_wifi_connect();
        } else if(g_ap_active && conn) {
            close_setup_ap();
        }
    }
}

void net_start(void) {
    g_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL));

    if(settings_has_wifi()) {
        g_mode = NET_MODE_STA;
        const settings_t* s = settings_get();
        wifi_config_t sta = {0};
        strncpy((char*)sta.sta.ssid, s->wifi_ssid, sizeof(sta.sta.ssid) - 1);
        strncpy((char*)sta.sta.password, s->wifi_pass, sizeof(sta.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_NONE);
        esp_wifi_set_max_tx_power(TX_POWER_QDBM);
        xTaskCreate(monitor_task, "netmon", 3072, NULL, 4, NULL);
        ESP_LOGI(TAG, "STA mode: connecting to %s", s->wifi_ssid);
    } else {
        g_mode = NET_MODE_AP;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_NONE);
        open_setup_ap();
    }
}

net_mode_t net_mode(void) { return g_mode; }

bool net_ap_active(void) { return g_ap_active; }

bool net_sta_connected(void) {
    return g_mode == NET_MODE_STA && (xEventGroupGetBits(g_evt) & BIT_CONNECTED);
}

void net_get_ip(char* buf, int n) {
    esp_netif_t* nif = esp_netif_get_handle_from_ifkey(
        net_sta_connected() ? "WIFI_STA_DEF" : "WIFI_AP_DEF");
    esp_netif_ip_info_t ip;
    if(nif && esp_netif_get_ip_info(nif, &ip) == ESP_OK) {
        snprintf(buf, n, IPSTR, IP2STR(&ip.ip));
    } else {
        strncpy(buf, "0.0.0.0", n);
    }
}

int net_rssi(void) {
    wifi_ap_record_t ap;
    if(net_sta_connected() && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) return ap.rssi;
    return 0;
}

int net_scan(char* out, int cap) {
    wifi_scan_config_t sc = {.show_hidden = false};
    if(esp_wifi_scan_start(&sc, true) != ESP_OK) return snprintf(out, cap, "{\"networks\":[]}");
    uint16_t num = 20;
    wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_records(&num, recs);
    int len = snprintf(out, cap, "{\"networks\":[");
    int added = 0;
    for(int i = 0; i < num && len < cap - 120; i++) {
        if(!recs[i].ssid[0] || strpbrk((char*)recs[i].ssid, "\"\\")) continue;
        bool dup = false;  // one entry per SSID (mesh nodes repeat it)
        for(int j = 0; j < i; j++)
            if(!strcmp((char*)recs[j].ssid, (char*)recs[i].ssid)) { dup = true; break; }
        if(dup) continue;
        len += snprintf(out + len, cap - len, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
                        added++ ? "," : "", (char*)recs[i].ssid, recs[i].rssi,
                        recs[i].authmode == WIFI_AUTH_OPEN ? "false" : "true");
    }
    len += snprintf(out + len, cap - len, "]}");
    return len;
}
