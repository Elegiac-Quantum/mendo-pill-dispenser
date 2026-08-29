#include "pill_audio/wav_reader.h"

#include <string.h>

enum {
    SAFE_RATE = 16000,
    SAFE_BYTE_RATE = 32000,
    SAFE_DATA_BYTES_MAX = SAFE_BYTE_RATE * 50,
};

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool pill_wav_parse(const uint8_t *header, size_t size, pill_wav_info_t *info)
{
    if (header == NULL || size < 20 || memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }
    uint32_t riff_end = le32(header + 4) + 8u;
    if (riff_end < 20) return false;

    bool format_valid = false;
    size_t offset = 12;
    while (offset <= size && size - offset >= 8) {
        const uint8_t *chunk = header + offset;
        uint32_t chunk_size = le32(chunk + 4);
        uint64_t payload_end = (uint64_t)offset + 8u + chunk_size;
        uint64_t padded_end = payload_end + (chunk_size & 1u);
        if (payload_end > riff_end || padded_end > UINT32_MAX) return false;

        if (memcmp(chunk, "fmt ", 4) == 0) {
            if (chunk_size < 16 || payload_end > size) return false;
            const uint8_t *format = chunk + 8;
            format_valid = le16(format) == 1 && le16(format + 2) == 1 &&
                           le32(format + 4) == SAFE_RATE &&
                           le32(format + 8) == SAFE_BYTE_RATE &&
                           le16(format + 12) == 2 && le16(format + 14) == 16;
            if (!format_valid) return false;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!format_valid || chunk_size == 0 || chunk_size > SAFE_DATA_BYTES_MAX ||
                (chunk_size & 1u) != 0 || riff_end < payload_end) {
                return false;
            }
            if (info != NULL) {
                *info = (pill_wav_info_t){
                    .data_offset = (uint32_t)offset + 8u,
                    .data_bytes = chunk_size,
                    .sample_rate = SAFE_RATE,
                    .duration_ms = chunk_size * 1000u / SAFE_BYTE_RATE,
                };
            }
            return true;
        }

        if (padded_end > size) return false;
        offset = (size_t)padded_end;
    }
    return false;
}
