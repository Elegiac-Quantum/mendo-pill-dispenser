idf_component_register(
    SRCS "clock_policy.c" "reminder_policy.c" "display_model.c" "app.c"
    INCLUDE_DIRS "include"
    REQUIRES board_support dose_controller pill_domain pill_ai_transport pill_audio
             schedule_store nvs_flash esp_event espressif__esp-sr
    EMBED_FILES "wake_ack.wav" "medicine_reminder.wav" "alarm_bell.wav"
)
