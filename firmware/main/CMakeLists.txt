idf_component_register(
    SRCS "main.c" "settings.c" "net.c" "web.c" "meter.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES "../../web/index.html" "../../web/style.css" "../../web/app.js"
    REQUIRES nartis_rf nvs_flash esp_wifi esp_http_server esp_netif lwip json driver esp_timer esp_app_format
)
