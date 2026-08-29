#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

bool board_rtc_decode(const uint8_t registers[7], struct tm *local_time);
bool board_rtc_encode(const struct tm *local_time, uint8_t registers[7]);
