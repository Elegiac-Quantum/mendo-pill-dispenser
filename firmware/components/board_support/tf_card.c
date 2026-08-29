#include "board_support/board.h"

#include "board_support/board_contract.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t *s_card;

esp_err_t board_tf_card_init(uint64_t *capacity_bytes)
{
    ESP_RETURN_ON_FALSE(capacity_bytes != NULL, ESP_ERR_INVALID_ARG,
                        "tf_card", "capacity output");
    *capacity_bytes = 0;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000;

    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.gpio_cs = BOARD_TF_CS_GPIO;
    device.host_id = SPI2_HOST;

    const esp_vfs_fat_mount_config_t mount = {
        .format_if_mount_failed = BOARD_TF_FORMAT_IF_MOUNT_FAILED,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdspi_mount("/sdcard", &host, &device,
                                                &mount, &s_card),
                        "tf_card", "read-only bring-up mount");
    *capacity_bytes = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
    return ESP_OK;
}
