#include "Clock.h"
#include <stdio.h>

namespace Clock {

static bool leap(uint16_t y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

uint8_t daysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t D[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    return (uint8_t)(D[month - 1] + (month == 2 && leap(year) ? 1 : 0));
}

bool valid(const DateTime& t) {
    if (t.year < 2000 || t.year > YEAR_MAX) return false;
    if (t.month < 1 || t.month > 12) return false;
    if (t.day < 1 || t.day > daysInMonth(t.year, t.month)) return false;
    return t.hour < 24 && t.minute < 60 && t.second < 60;
}

uint32_t toSeconds(const DateTime& t) {
    uint32_t days = 0;
    for (uint16_t y = 2000; y < t.year; y++) days += leap(y) ? 366 : 365;
    for (uint8_t m = 1; m < t.month; m++) days += daysInMonth(t.year, m);
    days += (uint32_t)(t.day - 1);
    return ((days * 24 + t.hour) * 60 + t.minute) * 60 + t.second;
}

DateTime fromSeconds(uint32_t s) {
    DateTime t;
    t.second = (uint8_t)(s % 60); s /= 60;
    t.minute = (uint8_t)(s % 60); s /= 60;
    t.hour = (uint8_t)(s % 24);
    uint32_t days = s / 24;
    uint16_t y = 2000;
    while (days >= (uint32_t)(leap(y) ? 366 : 365)) { days -= leap(y) ? 366 : 365; y++; }
    uint8_t m = 1;
    while (days >= daysInMonth(y, m)) { days -= daysInMonth(y, m); m++; }
    t.year = y;
    t.month = m;
    t.day = (uint8_t)(days + 1);
    return t;
}

bool parse(const char* text, DateTime& out) {
    unsigned y, mo, d, h, mi, se = 0;
    int n = sscanf(text, "%u-%u-%u %u:%u:%u", &y, &mo, &d, &h, &mi, &se);
    if (n < 5) return false;
    DateTime t = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)se };
    if (y < YEAR_MIN || !valid(t)) return false;
    out = t;
    return true;
}

}  // namespace Clock
