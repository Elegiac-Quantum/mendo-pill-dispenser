idf_component_register(
    SRCS "web_validation.c" "schedule_json.c" "caregiver_auth.c" "web.c"
    INCLUDE_DIRS "include"
    REQUIRES pill_domain pill_audio pill_ai_transport schedule_store board_support esp_http_server esp_http_client esp_wifi esp_netif nvs_flash json mbedtls
    EMBED_FILES "assets/index.html" "assets/styles.css" "assets/app.js"
)
