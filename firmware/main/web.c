#include "web.h"
#include "settings.h"
#include "net.h"
#include "meter.h"
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

static const char* TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

static esp_err_t send_asset(httpd_req_t* r, const char* type,
                            const uint8_t* start, const uint8_t* end) {
    httpd_resp_set_type(r, type);
    return httpd_resp_send(r, (const char*)start, end - start);
}

static esp_err_t h_index(httpd_req_t* r) {
    return send_asset(r, "text/html", index_html_start, index_html_end);
}
static esp_err_t h_css(httpd_req_t* r) {
    return send_asset(r, "text/css", style_css_start, style_css_end);
}
static esp_err_t h_js(httpd_req_t* r) {
    return send_asset(r, "application/javascript", app_js_start, app_js_end);
}

static char* read_body(httpd_req_t* r) {
    int len = r->content_len;
    if(len <= 0 || len > 1024) return NULL;
    char* buf = malloc(len + 1);
    if(!buf) return NULL;
    int got = 0;
    while(got < len) {
        int n = httpd_req_recv(r, buf + got, len - got);
        if(n <= 0) { free(buf); return NULL; }
        got += n;
    }
    buf[len] = 0;
    return buf;
}

static esp_err_t send_json(httpd_req_t* r, cJSON* j) {
    char* s = cJSON_PrintUnformatted(j);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Access-Control-Allow-Origin", "*");
    esp_err_t e = httpd_resp_send(r, s, strlen(s));
    free(s);
    cJSON_Delete(j);
    return e;
}

static esp_err_t h_readings(httpd_req_t* r) {
    meter_state_t* stp = malloc(sizeof(meter_state_t));
    if(!stp) { httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "oom"); return ESP_FAIL; }
    meter_snapshot(stp);
    const settings_t* s = settings_get();
    uint32_t now = esp_timer_get_time() / 1000000;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "nartis");
    cJSON_AddBoolToObject(root, "online", net_sta_connected() || net_ap_active());
    cJSON_AddBoolToObject(root, "paired", stp->paired);
    cJSON_AddStringToObject(root, "serial", s->meter_serial);
    if(stp->last_update_s) cJSON_AddNumberToObject(root, "age", now - stp->last_update_s);
    else cJSON_AddNullToObject(root, "age");
    cJSON* arr = cJSON_AddArrayToObject(root, "values");
    for(int i = 0; i < stp->count; i++) {
        cJSON* v = cJSON_CreateObject();
        cJSON_AddStringToObject(v, "label", stp->values[i].label);
        cJSON_AddStringToObject(v, "obis", stp->values[i].obis);
        cJSON_AddStringToObject(v, "unit", stp->values[i].unit);
        cJSON_AddNumberToObject(v, "decimals", stp->values[i].decimals);
        if(stp->values[i].valid) cJSON_AddNumberToObject(v, "value", stp->values[i].value);
        else cJSON_AddNullToObject(v, "value");
        cJSON_AddBoolToObject(v, "stale", stp->values[i].stale);
        cJSON_AddItemToArray(arr, v);
    }
    int nvalid = 0;
    for(int i = 0; i < stp->count; i++) nvalid += stp->values[i].valid;
    ESP_LOGD(TAG, "GET /api/readings: %d/%d valid, paired=%d, age=%lu", nvalid, stp->count, stp->paired,
             (unsigned long)(stp->last_update_s ? now - stp->last_update_s : 0));
    free(stp);
    return send_json(r, root);
}

static esp_err_t h_config_get(httpd_req_t* r) {
    const settings_t* s = settings_get();
    cJSON* j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "meter_serial", s->meter_serial);
    cJSON_AddStringToObject(j, "channel", s->channel);
    cJSON_AddNumberToObject(j, "update_interval", s->interval);
    cJSON_AddStringToObject(j, "wifi_ssid", s->wifi_ssid);
    return send_json(r, j);
}

static void reboot_task(void* a) { (void)a; vTaskDelay(pdMS_TO_TICKS(1200)); esp_restart(); }

static esp_err_t h_config_post(httpd_req_t* r) {
    char* body = read_body(r);
    if(!body) { httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "bad"); return ESP_FAIL; }
    cJSON* j = cJSON_Parse(body);
    free(body);
    if(!j) { httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json"); return ESP_FAIL; }
    const char* serial = cJSON_GetStringValue(cJSON_GetObjectItem(j, "meter_serial"));
    const char* channel = cJSON_GetStringValue(cJSON_GetObjectItem(j, "channel"));
    cJSON* iv = cJSON_GetObjectItem(j, "update_interval");
    settings_set_config(serial, channel, iv ? (uint32_t)iv->valuedouble : 0);
    cJSON_Delete(j);
    cJSON* ok = cJSON_CreateObject();
    cJSON_AddBoolToObject(ok, "ok", true);
    send_json(r, ok);
    xTaskCreate(reboot_task, "reboot", 2048, NULL, 5, NULL);  // restart radio with new config
    return ESP_OK;
}

static esp_err_t h_wifi_scan(httpd_req_t* r) {
    static char buf[2048];
    int len = net_scan(buf, sizeof(buf));
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_send(r, buf, len);
}


static esp_err_t h_wifi_post(httpd_req_t* r) {
    char* body = read_body(r);
    if(!body) { httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "bad"); return ESP_FAIL; }
    cJSON* j = cJSON_Parse(body);
    free(body);
    if(!j) { httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "json"); return ESP_FAIL; }
    const char* ssid = cJSON_GetStringValue(cJSON_GetObjectItem(j, "ssid"));
    const char* pass = cJSON_GetStringValue(cJSON_GetObjectItem(j, "password"));
    if(ssid && ssid[0]) settings_set_wifi(ssid, pass ? pass : "");
    cJSON_Delete(j);
    cJSON* ok = cJSON_CreateObject();
    cJSON_AddBoolToObject(ok, "ok", true);
    send_json(r, ok);
    xTaskCreate(reboot_task, "reboot", 2048, NULL, 5, NULL);  // reconnect with new creds
    return ESP_OK;
}

static esp_err_t h_status(httpd_req_t* r) {
    char ip[16];
    net_get_ip(ip, sizeof(ip));
    cJSON* j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "mode", net_mode() == NET_MODE_AP ? "ap" : (net_ap_active() ? "sta+ap" : "sta"));
    cJSON_AddStringToObject(j, "ip", ip);
    cJSON_AddBoolToObject(j, "connected", net_sta_connected());
    cJSON_AddNumberToObject(j, "rssi", net_rssi());
    return send_json(r, j);
}

// Captive-portal catch-all: push probe requests to the setup page.
static esp_err_t h_redirect(httpd_req_t* r) {
    if(!net_ap_active()) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "not found");
        return ESP_OK;
    }
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/");
    httpd_resp_send(r, NULL, 0);
    return ESP_OK;
}

static void reg(httpd_handle_t s, const char* uri, httpd_method_t m, esp_err_t (*h)(httpd_req_t*)) {
    httpd_uri_t u = {.uri = uri, .method = m, .handler = h};
    httpd_register_uri_handler(s, &u);
}

void web_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 16;
    cfg.stack_size = 10240;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t s = NULL;
    if(httpd_start(&s, &cfg) != ESP_OK) { ESP_LOGE(TAG, "httpd start failed"); return; }
    reg(s, "/", HTTP_GET, h_index);
    reg(s, "/style.css", HTTP_GET, h_css);
    reg(s, "/app.js", HTTP_GET, h_js);
    reg(s, "/api/readings", HTTP_GET, h_readings);
    reg(s, "/api/config", HTTP_GET, h_config_get);
    reg(s, "/api/config", HTTP_POST, h_config_post);
    reg(s, "/api/wifi/scan", HTTP_GET, h_wifi_scan);
    reg(s, "/api/wifi", HTTP_POST, h_wifi_post);
    reg(s, "/api/status", HTTP_GET, h_status);
    reg(s, "/*", HTTP_GET, h_redirect);  // captive portal
    ESP_LOGI(TAG, "web server up");
}
