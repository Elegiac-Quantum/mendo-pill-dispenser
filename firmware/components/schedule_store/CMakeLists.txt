idf_component_register(SRCS "schedule_store.c" "storage_nvs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES pill_domain nvs_flash)
