#include "board_support/rtc_decode.h"

#include <stddef.h>

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10U) + (value & 0x0fU));
}

static uint8_t to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

bool board_rtc_decode(const uint8_t registers[7], struct tm *local_time)
{
    if (registers == NULL || local_time == NULL) {
        return false;
    }
    const uint8_t second = from_bcd(registers[0] & 0x7fU);
    const uint8_t minute = from_bcd(registers[1] & 0x7fU);
    const uint8_t hour = from_bcd(registers[2] & 0x3fU);
    const uint8_t day = from_bcd(registers[4] & 0x3fU);
    const uint8_t month = from_bcd(registers[5] & 0x1fU);
    const uint16_t year = (uint16_t)(2000U + from_bcd(registers[6]));
    if (second > 59 || minute > 59 || hour > 23 || day < 1 || day > 31 ||
        month < 1 || month > 12 || year < 2024 || year > 2099) {
        return false;
    }
    *local_time = (struct tm){
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900,
        .tm_isdst = -1,
    };
    return true;
}

bool board_rtc_encode(const struct tm *local_time, uint8_t registers[7])
{
    if (local_time == NULL || registers == NULL ||
        local_time->tm_year < 124 || local_time->tm_year > 199 ||
        local_time->tm_mon < 0 || local_time->tm_mon > 11 ||
        local_time->tm_mday < 1 || local_time->tm_mday > 31 ||
        local_time->tm_hour < 0 || local_time->tm_hour > 23 ||
        local_time->tm_min < 0 || local_time->tm_min > 59 ||
        local_time->tm_sec < 0 || local_time->tm_sec > 59 ||
        local_time->tm_wday < 0 || local_time->tm_wday > 6) {
        return false;
    }
    registers[0] = to_bcd((uint8_t)local_time->tm_sec);
    registers[1] = to_bcd((uint8_t)local_time->tm_min);
    registers[2] = to_bcd((uint8_t)local_time->tm_hour);
    registers[3] = to_bcd((uint8_t)(local_time->tm_wday + 1));
    registers[4] = to_bcd((uint8_t)local_time->tm_mday);
    registers[5] = to_bcd((uint8_t)(local_time->tm_mon + 1));
    registers[6] = to_bcd((uint8_t)(local_time->tm_year - 100));
    return true;
}
