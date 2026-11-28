#pragma once
#include <stdint.h>

// ============================================================
//  공통: 날짜와 시각 계산 (RTC DS3231, 2026-11-28). 하드웨어를 부르지 않는다.
//  RTC에는 현지 시각을 그대로 넣는다 (시간대 없음).
//  초 = 2000-01-01 00:00:00부터 지난 초.
// ============================================================
struct DateTime {
    uint16_t year;      // 2000~2099
    uint8_t  month;     // 1~12
    uint8_t  day;       // 1~31
    uint8_t  hour;      // 0~23
    uint8_t  minute;
    uint8_t  second;
};

namespace Clock {
    const uint16_t YEAR_MIN = 2024, YEAR_MAX = 2099;
    uint8_t  daysInMonth(uint16_t year, uint8_t month);
    bool     valid(const DateTime& t);
    uint32_t toSeconds(const DateTime& t);
    DateTime fromSeconds(uint32_t s);
    // "YYYY-MM-DD HH:MM" 또는 "YYYY-MM-DD HH:MM:SS"를 읽는다
    bool     parse(const char* text, DateTime& out);
}
