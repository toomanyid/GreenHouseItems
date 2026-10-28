#include "io_Format.h"
#include <stdio.h>
#include <string.h>
#include "Settings.h"

namespace Fmt {

static void pad(char* out, uint8_t width, const char* s) {
    // 오른쪽 맞춤
    size_t n = strlen(s);
    if (n >= width) { memcpy(out, s + (n - width), width); out[width] = 0; return; }
    memset(out, ' ', width - n);
    memcpy(out + (width - n), s, n + 1);
}

static void put(char* line, uint8_t col, const char* s) {
    for (uint8_t i = 0; s[i] && col + i < 16; i++) line[col + i] = s[i];
}

static void blank(char* line, uint8_t n) {
    memset(line, ' ', n);
    line[n] = 0;
}

void secs(char* out, cs_t v, uint8_t width) {
    char b[16];
    if (v < 0) { pad(out, width, "---.-"); return; }
    cs_t r = (v + 25) / 50 * 50;            // 화면에만 0.5초로 맞춤
    snprintf(b, sizeof(b), "%ld.%ld", (long)(r / 100), (long)((r % 100) / 10));
    pad(out, width, b);
}

void secs2(char* out, cs_t v) {
    if (v < 0) { strcpy(out, "---.--"); return; }
    snprintf(out, 12, "%ld.%02ld", (long)(v / 100), (long)(v % 100));
}

void amps(char* out, int32_t ma) {
    if (ma < 0) { strcpy(out, "-.-A"); return; }
    int32_t d = (ma + 50) / 100;
    snprintf(out, 10, "%ld.%ldA", (long)(d / 10), (long)(d % 10));
}

void temp4(char* out, int16_t t) {
    char b[10];
    if (t < -99) t = -99;
    if (t > 999) t = 999;
    int a = t < 0 ? -t : t;
    snprintf(b, sizeof(b), "%s%d.%d", t < 0 ? "-" : "", a / 10, a % 10);
    pad(out, 4, b);
}

void pct4(char* out, pct_t p) {
    char b[8];
    if (p < 0) { strcpy(out, "--.-"); return; }
    if (p > PCT_MAX) p = PCT_MAX;
    snprintf(b, sizeof(b), "%d.%d", p / 10, p % 10);
    pad(out, 4, b);
}

void tenthsSigned(char* out, int32_t v) {
    long a = v < 0 ? -v : v;
    snprintf(out, 12, "%c%ld.%ld", v < 0 ? '-' : '+', a / 10, a % 10);
}

const char* faultLong(Cause c) {
    switch (c) {
    case Cause::OVERLOAD:    return "ERR OVERLOAD";
    case Cause::NO_CURRENT:  return "ERR NO CURR";
    case Cause::LIMIT_HIT:   return "ERR LIMIT";
    case Cause::DRIVER:      return "ERR DRIVER";
    case Cause::TIMEOUT:     return "ERR TIMEOUT";
    case Cause::SENSOR:      return "ERR SENSOR";
    case Cause::RELAY:       return "ERR RELAY";
    case Cause::HOME_SENSOR: return "ERR SENSOR";
    case Cause::TEMP_LOST:   return "ERR TEMP LOST";
    case Cause::TEMP_BAD:    return "ERR TEMP BAD";
    default:                 return "ERR";
    }
}

const char* causeLog(Cause c) {
    switch (c) {
    case Cause::HOME_SENSOR: return "HOME SENSOR";
    case Cause::TEMP_LOST:   return "TEMP LOST";
    case Cause::TEMP_BAD:    return "TEMP BAD";
    default:                 return faultLong(c);
    }
}

void buildDate(char* out, const char* date, const char* time) {
    static const char* M = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int month = 0;
    for (int i = 0; i < 12; i++) if (strncmp(date, M + i * 3, 3) == 0) { month = i + 1; break; }
    int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
    snprintf(out, 17, "%.4s-%02d-%02d %.5s", date + 7, month, day, time);
}

void splashTop(char* out, const char* name, uint8_t major, uint8_t minor) {
    blank(out, 16);
    put(out, 0, name);
    char v[8];
    snprintf(v, sizeof(v), "%02u.%02u", (unsigned)major, (unsigned)minor);
    put(out, 11, v);
}

char statusChar(const WindowStatus& w, bool blinkOn) {
    switch (w.state) {
    case WinState::FAULT:        return blinkOn ? '!' : ' ';
    case WinState::LIMIT:        return 'L';
    case WinState::REVERSE_WAIT: return w.nextDir > 0 ? '^' : (w.nextDir < 0 ? 'v' : ' ');
    case WinState::HOMING:
    case WinState::MOVING:
    case WinState::CLOSING:
    case WinState::CALIBRATING: {
        int8_t d = w.dir != 0 ? w.dir : w.nextDir;
        if (d == 0 || !blinkOn) return ' ';
        return d > 0 ? '^' : 'v';
    }
    default:
        return w.posState == PosState::UNKNOWN ? '?' : ' ';
    }
}

pct_t winPct(const WindowStatus& w) {
    if (w.posState == PosState::CLOSED) return PCT_CLOSE;
    if (w.posState == PosState::UNKNOWN) return PCT_UNKNOWN;
    return SettingsRules::pctFromPos(w.pos, w.maxPos);
}

pct_t targetPct(const WindowStatus& w) {
    if (w.target < 0) return PCT_CLOSE;
    return SettingsRules::pctFromPos(w.target, w.maxPos);
}

void smallLine(char* out, const WindowStatus& w, uint8_t num, bool blinkOn, bool sensorErr) {
    char b[12];
    blank(out, 16);
    out[0] = (char)('0' + num);
    out[15] = statusChar(w, blinkOn);

    if (w.state == WinState::FAULT) { put(out, 2, faultLong(w.fault)); return; }
    if (sensorErr) { put(out, 2, "ERR SENSOR"); return; }

    switch (w.state) {
    case WinState::HOMING:
        put(out, 2, "HOME");
        out[7] = '>';
        if (w.target < 0) put(out, 8, "CLOSE");
        else { pct4(b, targetPct(w)); put(out, 8, b); }
        return;
    case WinState::CALIBRATING:
        put(out, 2, "CAL");
        secs(b, w.cal.elapsed, 5); put(out, 6, b); put(out, 11, "s");
        return;
    default:
        break;
    }
    pct_t p = winPct(w);
    if (p == PCT_CLOSE) put(out, 2, "CLOSE");
    else { pct4(b, p); put(out, 2, b); }
    bool moving = w.state == WinState::MOVING || w.state == WinState::CLOSING || w.state == WinState::REVERSE_WAIT;
    if (moving && p != PCT_UNKNOWN) {
        uint8_t col = p == PCT_CLOSE ? 7 : 6;
        out[col] = '>';
        if (w.target < 0) put(out, (uint8_t)(col + 1), "CLOSE");
        else { pct4(b, targetPct(w)); put(out, (uint8_t)(col + 1), b); }
    }
}

// ------------------------------------------------------------
//  큰 글씨
// ------------------------------------------------------------
const uint8_t BIG_GLYPHS[BIG_GLYPH_COUNT][8] = {
    { 31, 31, 31, 0, 0, 0, 0, 0 },        // 위 막대
    { 0, 0, 0, 0, 0, 31, 31, 31 },        // 아래 막대
    { 31, 31, 31, 0, 0, 31, 31, 31 },     // 위아래 막대
    { 0, 0, 0, 0, 0, 0, 14, 14 },         // 소수점
    { 2, 2, 31, 4, 31, 8, 8, 0 },         // ≠
};

// 숫자마다 윗줄 3칸, 아랫줄 3칸. F 꽉 참, T 위 막대, B 아래 막대, U 위아래 막대
static const char* const BIG_DIGITS[10][2] = {
    { "FTF", "FBF" }, { "TF ", "BFB" }, { "UUF", "FBB" }, { "UUF", "BBF" }, { "FBF", "  F" },
    { "FUU", "BBF" }, { "FUU", "FBF" }, { "TTF", "  F" }, { "FUF", "FBF" }, { "FUF", "BBF" },
};
static const char* const BIG_DASH[2] = { "BBB", "   " };

static char bigCode(char k) {
    switch (k) {
    case 'F': return G_FULL;
    case 'T': return G_TOP;
    case 'B': return G_BOT;
    case 'U': return G_BOTH;
    default:  return ' ';
    }
}

static void bigDigit(char* row0, char* row1, uint8_t col, const char* const g[2]) {
    for (uint8_t k = 0; k < 3; k++) {
        row0[col + k] = bigCode(g[0][k]);
        row1[col + k] = bigCode(g[1][k]);
    }
}

void bigNumber(char* row0, char* row1, int16_t tenths) {
    for (uint8_t c = 0; c < 11; c++) { row0[c] = ' '; row1[c] = ' '; }
    row1[7] = G_DOT;
    if (tenths < 0) {
        bigDigit(row0, row1, 0, BIG_DASH);
        bigDigit(row0, row1, 4, BIG_DASH);
        bigDigit(row0, row1, 8, BIG_DASH);
        return;
    }
    if (tenths > 999) tenths = 999;
    uint8_t tens = (uint8_t)(tenths / 100), ones = (uint8_t)(tenths / 10 % 10), dec = (uint8_t)(tenths % 10);
    if (tens) bigDigit(row0, row1, 0, BIG_DIGITS[tens]);     // 10 아래는 앞자리를 비운다
    bigDigit(row0, row1, 4, BIG_DIGITS[ones]);
    bigDigit(row0, row1, 8, BIG_DIGITS[dec]);
}

// ------------------------------------------------------------
//  가로 두 배 숫자
// ------------------------------------------------------------
// 7획: a 위, b 오른쪽 위, c 오른쪽 아래, d 아래, e 왼쪽 아래, f 왼쪽 위, g 가운데
static const uint8_t SEGS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,   // 비트 0 = a ... 비트 6 = g
};

void wideHalf(uint8_t digit, uint8_t side, uint8_t out[8]) {
    uint16_t g[8] = {};      // 10점, 비트 9가 맨 왼쪽
    uint8_t s = SEGS[digit % 10];
    auto set = [&](uint8_t y0, uint8_t y1, uint8_t x0, uint8_t x1) {
        for (uint8_t y = y0; y <= y1; y++)
            for (uint8_t x = x0; x <= x1; x++) g[y] |= (uint16_t)(1u << (9 - x));
    };
    if (s & 0x01) set(0, 0, 1, 9);   // a
    if (s & 0x40) set(3, 3, 1, 9);   // g
    if (s & 0x08) set(6, 6, 1, 9);   // d
    if (s & 0x20) set(0, 3, 1, 2);   // f
    if (s & 0x10) set(3, 6, 1, 2);   // e
    if (s & 0x02) set(0, 3, 8, 9);   // b
    if (s & 0x04) set(3, 6, 8, 9);   // c
    for (uint8_t y = 0; y < 8; y++)
        out[y] = (uint8_t)(side == 0 ? (g[y] >> 5) & 0x1F : g[y] & 0x1F);
}

void wideAssign(WideSlots& s, const bool need[10]) {
    bool used[4] = { false, false, false, false };
    bool placed[10] = {};
    for (uint8_t p = 0; p < 4; p++) {
        int8_t d = s.digit[p];
        if (d >= 0 && d <= 9 && need[d] && !placed[d]) { used[p] = true; placed[d] = true; }
        else s.digit[p] = -1;
    }
    for (uint8_t d = 0; d < 10; d++) {
        if (!need[d] || placed[d]) continue;
        for (uint8_t p = 0; p < 4; p++) {
            if (used[p]) continue;
            s.digit[p] = (int8_t)d;
            used[p] = true;
            placed[d] = true;
            break;
        }
    }
}

void wideNeed(bool need[10], pct_t p) {
    if (p < 0) return;
    if (p > PCT_MAX) p = PCT_MAX;
    if (p >= 100) need[p / 100] = true;
    need[p / 10 % 10] = true;
}

static void wideDigit(char* out, uint8_t col, uint8_t d, const WideSlots& s) {
    for (uint8_t q = 0; q < 4; q++) {
        if (s.digit[q] != (int8_t)d) continue;
        out[col] = (char)(G_TOP + 2 * q);
        out[col + 1] = (char)(G_TOP + 2 * q + 1);
        return;
    }
    out[col] = (char)('0' + d);      // 칸이 모자라면 작은 숫자로 (그럴 일은 없다)
}

void wideLine(char* out, uint8_t num, pct_t p, const WideSlots& s, bool showValue, const char* extra) {
    blank(out, 16);
    out[0] = (char)('0' + num);
    if (extra) put(out, 9, extra);
    if (!showValue) return;
    if (p < 0) { put(out, 4, "--.-"); return; }
    if (p > PCT_MAX) p = PCT_MAX;
    if (p >= 100) wideDigit(out, 2, (uint8_t)(p / 100), s);
    wideDigit(out, 4, (uint8_t)(p / 10 % 10), s);
    out[6] = '.';
    out[7] = (char)('0' + p % 10);
}

}  // namespace Fmt
