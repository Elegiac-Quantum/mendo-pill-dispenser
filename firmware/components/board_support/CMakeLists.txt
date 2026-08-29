idf_component_register(
    SRCS "board_contract.c" "board.c" "button.c" "audio_es8388.c" "xl9555.c" "display_renderer.c" "display_st7789.c" "tf_card.c" "servo.c" "rtc_decode.c" "rtc_ds3231.c" "mt6701_soft_i2c.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_driver_spi esp_lcd esp_timer esp_adc fatfs sdmmc esp_codec_dev nvs_flash
)
