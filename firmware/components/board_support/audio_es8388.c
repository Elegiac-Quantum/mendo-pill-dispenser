#include "board_support/audio.h"

#include "board_support/board.h"
#include "board_support/board_contract.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

static const char *TAG = "board_audio";
static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static esp_codec_dev_handle_t s_codec;
static i2c_master_dev_handle_t s_diagnostic_i2c;
static bool s_ready;
static uint32_t s_write_calls;
static uint32_t s_frames_submitted;
static int s_last_write_result;

esp_err_t board_audio_init(void)
{
    if (s_ready) return ESP_OK;

    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    esp_err_t error = i2s_new_channel(&channel, &s_tx, &s_rx);
    if (error != ESP_OK) return error;

    i2s_std_config_t standard = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BOARD_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_AUDIO_MCLK_GPIO,
            .bclk = BOARD_AUDIO_BCLK_GPIO,
            .ws = BOARD_AUDIO_LRCLK_GPIO,
            .dout = BOARD_AUDIO_DATA_OUT_GPIO,
            .din = BOARD_AUDIO_DATA_IN_GPIO,
        },
    };
    standard.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    error = i2s_channel_init_std_mode(s_tx, &standard);
    if (error != ESP_OK) return error;
    error = i2s_channel_init_std_mode(s_rx, &standard);
    if (error != ESP_OK) return error;

    audio_codec_i2s_cfg_t i2s_cfg = {.tx_handle = s_tx, .rx_handle = s_rx};
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr = ES8388_CODEC_DEFAULT_ADDR,
        .bus_handle = board_i2c_bus(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (data_if == NULL || ctrl_if == NULL || gpio_if == NULL) return ESP_ERR_NO_MEM;

    es8388_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .master_mode = false,
        .pa_pin = -1,
    };
    const audio_codec_if_t *codec_if = es8388_codec_new(&codec_cfg);
    if (codec_if == NULL) return ESP_FAIL;

    esp_codec_dev_cfg_t device = {
        .codec_if = codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
    };
    s_codec = esp_codec_dev_new(&device);
    if (s_codec == NULL) return ESP_ERR_NO_MEM;

    esp_codec_dev_sample_info_t sample = {
        .sample_rate = BOARD_AUDIO_SAMPLE_RATE,
        .channel = 2,
        .bits_per_sample = BOARD_AUDIO_BITS,
    };
    if (esp_codec_dev_set_out_vol(s_codec, 0) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_open(s_codec, &sample) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_in_gain(s_codec, 24.0f) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_out_mute(s_codec, true) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "ES8388 open failed");
        return ESP_FAIL;
    }
    const i2c_device_config_t diagnostic_i2c = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x10,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(board_i2c_bus(), &diagnostic_i2c,
                                  &s_diagnostic_i2c) != ESP_OK) {
        return ESP_FAIL;
    }
    const uint8_t speaker_left_plus_3db[] = {0x30, 0x21};
    const uint8_t speaker_right_plus_3db[] = {0x31, 0x21};
    if (i2c_master_transmit(s_diagnostic_i2c, speaker_left_plus_3db,
                            sizeof(speaker_left_plus_3db), 100) != ESP_OK ||
        i2c_master_transmit(s_diagnostic_i2c, speaker_right_plus_3db,
                            sizeof(speaker_right_plus_3db), 100) != ESP_OK) {
        return ESP_FAIL;
    }
    s_ready = true;
    ESP_LOGI(TAG, "ES8388 ready and muted");
    return ESP_OK;
}

esp_err_t board_audio_set_volume(uint8_t percent)
{
    if (!s_ready || !board_audio_config_valid(BOARD_AUDIO_SAMPLE_RATE,
                                               BOARD_AUDIO_BITS,
                                               BOARD_AUDIO_CHANNELS,
                                               percent)) return ESP_ERR_INVALID_ARG;
    if (esp_codec_dev_set_out_vol(s_codec, percent) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    return esp_codec_dev_set_out_mute(s_codec, percent == 0) == ESP_CODEC_DEV_OK
               ? ESP_OK : ESP_FAIL;
}

esp_err_t board_audio_write(const int16_t *mono, size_t frames, size_t *written)
{
    if (!s_ready || mono == NULL || frames == 0 || frames > 256) return ESP_ERR_INVALID_ARG;
    int16_t stereo[512];
    for (size_t i = 0; i < frames; ++i) {
        /* The delivered board's small speaker remains quiet at codec 100%.
         * Add a final 21.6 dB boost for AI, reminders and uploaded audio alike,
         * with saturation so loud samples cannot wrap into harsh distortion. */
        int32_t amplified = (int32_t)mono[i] * 12;
        if (amplified > INT16_MAX) amplified = INT16_MAX;
        if (amplified < INT16_MIN) amplified = INT16_MIN;
        stereo[i * 2] = (int16_t)amplified;
        stereo[i * 2 + 1] = (int16_t)amplified;
    }
    int result = esp_codec_dev_write(s_codec, stereo, (int)(frames * 4));
    s_write_calls++;
    s_last_write_result = result;
    if (result == ESP_CODEC_DEV_OK) s_frames_submitted += frames;
    if (written != NULL) *written = result == ESP_CODEC_DEV_OK ? frames : 0;
    return result == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t board_audio_read(int16_t *mono, size_t frames, size_t *read_frames)
{
    if (!s_ready || mono == NULL || frames == 0 || frames > 320) {
        return ESP_ERR_INVALID_ARG;
    }
    int16_t stereo[640];
    int result = esp_codec_dev_read(s_codec, stereo, (int)(frames * 4));
    if (result != ESP_CODEC_DEV_OK) {
        if (read_frames != NULL) *read_frames = 0;
        return ESP_FAIL;
    }
    for (size_t i = 0; i < frames; ++i) mono[i] = stereo[i * 2];
    if (read_frames != NULL) *read_frames = frames;
    return ESP_OK;
}

esp_err_t board_audio_read_stereo(int16_t *left, int16_t *right,
                                  size_t frames, size_t *read_frames)
{
    if (!s_ready || left == NULL || right == NULL || frames == 0 || frames > 320) {
        return ESP_ERR_INVALID_ARG;
    }
    int16_t stereo[640];
    int result = esp_codec_dev_read(s_codec, stereo, (int)(frames * 4));
    if (result != ESP_CODEC_DEV_OK) {
        if (read_frames != NULL) *read_frames = 0;
        return ESP_FAIL;
    }
    for (size_t i = 0; i < frames; ++i) {
        left[i] = stereo[i * 2];
        right[i] = stereo[i * 2 + 1];
    }
    if (read_frames != NULL) *read_frames = frames;
    return ESP_OK;
}

esp_err_t board_audio_stop(void)
{
    return board_audio_set_volume(0);
}

bool board_audio_ready(void)
{
    return s_ready;
}

esp_err_t board_audio_get_diagnostics(board_audio_diagnostics_t *d)
{
    if (!s_ready || d == NULL || s_diagnostic_i2c == NULL) return ESP_ERR_INVALID_ARG;
    *d = (board_audio_diagnostics_t){
        .write_calls = s_write_calls,
        .frames_submitted = s_frames_submitted,
        .last_write_result = s_last_write_result,
    };
    const uint8_t registers[] = {0x02, 0x04, 0x17, 0x18, 0x1a, 0x1b,
                                 0x1d, 0x27, 0x2a, 0x30, 0x31};
    int *values[] = {&d->reg_chip_power, &d->reg_dac_power, &d->reg_dac_format,
                     &d->reg_dac_rate, &d->reg_left_volume, &d->reg_right_volume,
                     &d->reg_mute, &d->reg_left_mixer, &d->reg_right_mixer,
                     &d->reg_speaker_left, &d->reg_speaker_right};
    for (size_t i = 0; i < sizeof(registers); ++i) {
        uint8_t value = 0;
        if (i2c_master_transmit_receive(s_diagnostic_i2c, &registers[i], 1,
                                        &value, 1, 100) != ESP_OK) return ESP_FAIL;
        *values[i] = value;
    }
    return ESP_OK;
}
