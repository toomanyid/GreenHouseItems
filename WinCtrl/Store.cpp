#include <Arduino.h>
#include <Preferences.h>
#include "Store.h"
#include "Settings.h"
#include <string.h>

static Preferences s_prefs;
static const char* NS = "winctrl";

namespace Store {

void begin() {
    s_prefs.begin(NS, false);
}

bool loadSettings(Settings& s) {
    // 옛 버전은 한 버전씩 옮겨 읽고 바로 다시 저장한다. 읽을 수 없으면 기본값 (스펙 8절, 2026-11-28)
    static uint8_t raw[SETTINGS_RAW_MAX];
    size_t n = s_prefs.getBytesLength("set");
    if (n > 0 && n <= sizeof(raw) && s_prefs.getBytes("set", raw, n) == n) {
        uint16_t ver = 0;
        memcpy(&ver, raw, n >= sizeof(ver) ? sizeof(ver) : 0);
        SettingsRules::LoadResult r = SettingsRules::load(raw, n, s);
        if (r == SettingsRules::LOAD_MIGRATED) saveSettings(s);
        if (r != SettingsRules::LOAD_NONE) {
            Serial.printf("[set] settings v%u%s, MC %s, home sens %s/%s, temp sens %s, notice %s\n",
                          (unsigned)SETTINGS_VERSION, r == SettingsRules::LOAD_MIGRATED ? " (migrated)" : "",
                          s.relayUse ? "USED" : "NONE", s.win[0].homeSensAuto ? "AUTO" : "OFF",
                          s.win[1].homeSensAuto ? "AUTO" : "OFF", s.temp.sensorAuto ? "AUTO" : "OFF",
                          s.basicAcked ? "acked" : "pending");
            if (r == SettingsRules::LOAD_MIGRATED) Serial.printf("[set] migrated from v%u\n", (unsigned)ver);
            return true;
        }
        Serial.printf("[set] stored settings unreadable (v%u, %u bytes): defaults\n", (unsigned)ver, (unsigned)n);
    } else {
        Serial.println("[set] no stored settings: defaults (basic config)");
    }
    SettingsRules::defaults(s);
    return false;
}

bool saveSettings(const Settings& s) {
    return s_prefs.putBytes("set", &s, sizeof(Settings)) == sizeof(Settings);
}

void loadErrors(ErrorLog& log) {
    log.clear();
    size_t n = s_prefs.getBytesLength("elog");
    if (n != ErrorLog::rawSize()) return;
    uint8_t cnt = s_prefs.getUChar("ecnt", 0);
    if (s_prefs.getBytes("elog", log.raw(), ErrorLog::rawSize()) != ErrorLog::rawSize()) { log.clear(); return; }
    log.setCount(cnt);
}

bool saveErrors(const ErrorLog& log) {
    ErrorLog& l = const_cast<ErrorLog&>(log);
    bool ok = s_prefs.putBytes("elog", l.raw(), ErrorLog::rawSize()) == ErrorLog::rawSize();
    ok = s_prefs.putUChar("ecnt", log.count()) == 1 && ok;
    return ok;
}

uint8_t loadUiItem(uint8_t dflt) {
    return s_prefs.getUChar("uiitem", dflt);
}

bool saveUiItem(uint8_t item) {
    return s_prefs.putUChar("uiitem", item) == 1;
}

uint16_t bumpBootCount() {
    uint16_t n = (uint16_t)(s_prefs.getUShort("boot", 0) + 1);
    s_prefs.putUShort("boot", n);
    return n;
}

}  // namespace Store
