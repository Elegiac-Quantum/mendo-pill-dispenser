idf_component_register(
    SRCS "policy.c" "config_validation.c" "config_nvs.c" "wire_protocol.c"
         "transport_ws.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash json esp_http_client espressif__esp_websocket_client mbedtls
             board_support pill_audio 78__esp-opus
)
