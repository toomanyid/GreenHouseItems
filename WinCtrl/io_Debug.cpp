#include <Arduino.h>
#include <string.h>
#include <ctype.h>
#include "io_Debug.h"

namespace Dbg {

static volatile uint8_t s_level = BASIC;   // 켤 때는 늘 1 (저장하지 않음)
static char    s_line[24];
static uint8_t s_len = 0;

uint8_t level() { return s_level; }

static const char* levelName(uint8_t l) {
    return l >= DETAIL ? "DETAIL" : (l >= ACTION ? "ACTION" : "BASIC");
}

static void help() {
    Serial.printf("[dbg] level %u (%s). commands: d1 basic, d2 action, d3 detail, d? help\n",
                  (unsigned)s_level, levelName(s_level));
}

bool command(const char* line) {
    // 앞뒤 빈칸을 빼고 소문자로
    char c[sizeof(s_line)];
    uint8_t n = 0;
    for (const char* p = line; *p && n < sizeof(c) - 1; p++) {
        if (*p == ' ' || *p == '\t') continue;
        c[n++] = (char)tolower((unsigned char)*p);
    }
    c[n] = 0;
    if (n == 0) return false;
    if (strcmp(c, "d") == 0 || strcmp(c, "d?") == 0) { help(); return true; }
    if (n == 2 && c[0] == 'd' && c[1] >= '1' && c[1] <= '3') {
        s_level = (uint8_t)(c[1] - '0');
        Serial.printf("[dbg] level %u (%s)\n", (unsigned)s_level, levelName(s_level));
        return true;
    }
    Serial.printf("[dbg] unknown command '%s'. d1 d2 d3 d?\n", c);
    return false;
}

void poll() {
    while (Serial.available() > 0) {
        int ch = Serial.read();
        if (ch < 0) break;
        if (ch == '\r' || ch == '\n') {
            if (s_len > 0) {
                s_line[s_len] = 0;
                command(s_line);
                s_len = 0;
            }
            continue;
        }
        if (s_len < sizeof(s_line) - 1) s_line[s_len++] = (char)ch;
    }
}

}  // namespace Dbg
