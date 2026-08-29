idf_component_register(SRCS "dose_controller.c" "storage_nvs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES pill_domain nvs_flash esp_hw_support)
