#include <assert.h>
#include <stdio.h>

#include "board_support/rtc_decode.h"

static void test_valid_bcd_registers(void)
{
    const uint8_t registers[7] = {0x56, 0x34, 0x12, 0x02, 0x21, 0x07, 0x26};
    struct tm value = {0};
    assert(board_rtc_decode(registers, &value));
    assert(value.tm_year == 126);
    assert(value.tm_mon == 6);
    assert(value.tm_mday == 21);
    assert(value.tm_hour == 12);
    assert(value.tm_min == 34);
    assert(value.tm_sec == 56);
}

static void test_unset_factory_date_is_invalid_not_transport_failure(void)
{
    const uint8_t registers[7] = {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00};
    struct tm value = {0};
    assert(!board_rtc_decode(registers, &value));
}

static void test_encode_local_time(void)
{
    struct tm source = {
        .tm_year = 126, .tm_mon = 6, .tm_mday = 26,
        .tm_hour = 9, .tm_min = 8, .tm_sec = 7, .tm_wday = 0,
    };
    uint8_t encoded[7] = {0};
    assert(board_rtc_encode(&source, encoded));
    assert(encoded[0] == 0x07 && encoded[1] == 0x08 && encoded[2] == 0x09);
    assert(encoded[3] == 0x01 && encoded[4] == 0x26);
    assert(encoded[5] == 0x07 && encoded[6] == 0x26);
    source.tm_year = 123;
    assert(!board_rtc_encode(&source, encoded));
}

int main(void)
{
    test_valid_bcd_registers();
    test_unset_factory_date_is_invalid_not_transport_failure();
    test_encode_local_time();
    puts("RTC decode tests passed");
    return 0;
}
