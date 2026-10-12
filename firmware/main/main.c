#include "nvs_flash.h"
#include "esp_log.h"
#include "settings.h"
#include "net.h"
#include "web.h"
#include "meter.h"

void app_main(void) {
    esp_err_t e = nvs_flash_init();
    if(e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    settings_init();
    net_start();
    meter_start();
    web_start();
    ESP_LOGI("main", "nartis up (%s)", settings_has_wifi() ? "STA" : "AP setup");
}
