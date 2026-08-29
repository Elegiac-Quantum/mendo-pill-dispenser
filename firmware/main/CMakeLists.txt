idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES board_support pill_ai_transport pill_app pill_web nvs_flash esp_netif esp_event spi_flash esp_driver_uart esp_adc
)
