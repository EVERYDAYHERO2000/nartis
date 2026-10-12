# Nartis RF meter stack (CC1101 + pairing + AES-GCM + DLMS), ported from
# FerrumLogic/esphome-nartis-rf-meter-cc1101 with a thin ESPHome shim.
idf_component_register(
    SRCS "src/cc1101_hal.cpp" "src/cc1101_vendored.c" "src/dlms_client.cpp"
         "src/nartis_rf_meter.cpp" "src/rf_data.cpp" "shim/esphome_shim.cpp"
    INCLUDE_DIRS "src" "shim"
    REQUIRES driver nvs_flash esp_timer esp_hw_support log
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-function -Wno-missing-field-initializers)
