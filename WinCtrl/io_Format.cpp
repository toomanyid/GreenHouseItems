#include "io_Format.h"
#include <stdio.h>
#include <string.h>

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

void secsInt(char* out, cs_t v, uint8_t width) {
    char b[12];
    if (v < 0) { pad(out, width, "---"); return; }
    snprintf(b, sizeof(b), "%ld", (long)((v + 50) / 100));
    pad(out, width, b);
}

uint8_t pctValue(cs_t pos, cs_t maxPos) {
    if (maxPos <= 0) return 1;
    if (pos < 0) pos = 0;
    int32_t p = 1 + (int32_t)((int64_t)pos * 99 / maxPos);
    if (p > 100) p = 100;
    return (uint8_t)p;
}

void pct(char* out, cs_t pos, cs_t maxPos) {
    char b[8];
    snprintf(b, sizeof(b), "%u", (unsigned)pctValue(pos, maxPos));
    pad(out, 3, b);
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

const char* faultShort(Cause c) {
    switch (c) {
    case Cause::OVERLOAD:    return "E-OVL";
    case Cause::NO_CURRENT:  return "E-NOC";
    case Cause::LIMIT_HIT:   return "E-LIM";
    case Cause::DRIVER:      return "E-DRV";
    case Cause::TIMEOUT:     return "E-TMO";
    case Cause::SENSOR:      return "E-SEN";
    case Cause::RELAY:       return "E-RLY";
    case Cause::HOME_SENSOR: return "E-SEN";
    case Cause::TEMP_LOST:   return "E-TMP";
    case Cause::TEMP_BAD:    return "E-TMP";
    default:                 return "E-???";
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

// 위치 칸: 초 5칸 + 's' + 빈칸 + 백분율 3칸 + '%' (3번 칸부터)
static void posField(char* line, PosState ps, cs_t pos, cs_t maxPos) {
    char b[12];
    if (ps == PosState::UNKNOWN) { put(line, 3, "---.-s ---%"); return; }
    if (ps == PosState::CLOSED)  { put(line, 3, " CLOSE   0%"); return; }
    secs(b, pos, 5); put(line, 3, b); put(line, 8, "s");
    pct(b, pos, maxPos); put(line, 10, b); put(line, 13, "%");
}

void winLine(char* out, const WindowStatus& w, const LineCtx& c) {
    char b[16];
    blank(out, 16);
    out[0] = c.sel;
    out[1] = (char)('0' + c.num);

    if (c.editing) {
        if (c.editTarget < 0) posField(out, PosState::CLOSED, 0, w.maxPos);
        else                  posField(out, PosState::VALID, c.editTarget, w.maxPos);
        return;
    }
    out[15] = statusChar(w, c.blinkOn);

    if (w.state == WinState::FAULT) { put(out, 3, faultLong(w.fault)); return; }
    if (c.sensorErr && c.altPhase) { put(out, 3, "ERR SENSOR"); return; }

    switch (w.state) {
    case WinState::HOMING:
        put(out, 3, "HOME");
        return;
    case WinState::CALIBRATING:
        put(out, 3, "CAL");
        secs(b, w.cal.elapsed, 5); put(out, 7, b); put(out, 12, "s");
        return;
    case WinState::MOVING:
    case WinState::CLOSING:
    case WinState::REVERSE_WAIT:
        if (w.posState != PosState::UNKNOWN) {
            if (w.posState == PosState::CLOSED) put(out, 3, "CLOSE");
            else { secs(b, w.pos, 5); put(out, 3, b); }
            out[8] = '>';
            if (w.target < 0) put(out, 9, "CLOSE");
            else { secs(b, w.target, 5); put(out, 9, b); }
            return;
        }
        posField(out, w.posState, w.pos, w.maxPos);
        return;
    default:
        posField(out, w.posState, w.pos, w.maxPos);
        return;
    }
}

// 오른쪽 8칸: 선택, 번호, 초 4칸 + 's' + 상태 글자, 또는 3번 칸부터 문구 5칸
void winShort(char* out, const WindowStatus& w, const LineCtx& c) {
    char b[12];
    blank(out, 8);
    out[0] = c.sel;
    out[1] = (char)('0' + c.num);

    if (c.editing) {
        if (c.editTarget < 0) { memcpy(out + 3, "CLOSE", 5); return; }
        secsInt(b, c.editTarget, 4); memcpy(out + 2, b, 4); out[6] = 's';
        return;
    }

    const char* phrase = nullptr;
    bool alternate = true;
    cs_t shown = w.posState == PosState::VALID ? w.pos : CS_EMPTY;

    if (w.state == WinState::FAULT)            phrase = faultShort(w.fault);
    else if (c.sensorErr)                      phrase = "E-SEN";
    else if (w.state == WinState::HOMING)      phrase = "HOME";
    else if (w.state == WinState::CALIBRATING) { phrase = "CAL"; shown = w.cal.elapsed; }

    if (w.posState == PosState::CLOSED && w.state != WinState::FAULT && !(c.sensorErr && c.altPhase)
        && w.state != WinState::CALIBRATING && w.state != WinState::HOMING) {
        memcpy(out + 3, "CLOSE", 5);           // CLOSE는 위치 자체라 번갈지 않음
        return;
    }
    if (phrase && (c.altPhase || !alternate)) {
        size_t n = strlen(phrase);
        memcpy(out + 3, phrase, n > 5 ? 5 : n);
        return;
    }
    secsInt(b, shown, 4); memcpy(out + 2, b, 4); out[6] = 's';
    out[7] = statusChar(w, c.blinkOn);
}

void tempLine(char* out, char sel, bool valid, int16_t t, char suffix, const char* right8) {
    char b[8];
    blank(out, 16);
    out[0] = sel;
    if (valid) { temp4(b, t); memcpy(out + 1, b, 4); }
    else       memcpy(out + 1, "--.-", 4);
    out[5] = suffix;
    memcpy(out + 8, right8, 8);
}

}  // namespace Fmt
