idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "inc"
    EMBED_FILES "wwwroot/index.html"
    REQUIRES "app_update" "esp_http_server" "esp_wifi" "wpa_supplicant" "nvs_flash" "esp_netif" "esp_event" "esp_timer" "freertos" "driver" "lwip" "esp_adc"
)