#include <Arduino.h>
#include <Preferences.h>
#include "Store.h"
#include "Settings.h"

static Preferences s_prefs;
static const char* NS = "winctrl";

namespace Store {

void begin() {
    s_prefs.begin(NS, false);
}

bool loadSettings(Settings& s) {
    // 저장 모양 버전이 다르면 예전 설정은 읽지 않고 기본값으로 시작한다 (스펙 8절, 2026-11-22)
    Settings tmp;
    if (s_prefs.getBytesLength("set") == sizeof(Settings)
        && s_prefs.getBytes("set", &tmp, sizeof(tmp)) == sizeof(tmp)
        && tmp.version == SETTINGS_VERSION && SettingsRules::valid(tmp)) {
        s = tmp;
        return true;
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
