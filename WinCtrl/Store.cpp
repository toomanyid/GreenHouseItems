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
    Settings tmp;
    size_t n = s_prefs.getBytesLength("set");
    if (n == sizeof(Settings) && s_prefs.getBytes("set", &tmp, sizeof(tmp)) == sizeof(tmp)
        && tmp.version == SETTINGS_VERSION) {
        // 센서 감지 전이므로 리밋 검사는 느슨한 쪽(센서 없음/있음 둘 중 하나라도 맞으면)으로 본다
        bool on[2] = {true, true}, off[2] = {false, false};
        if (SettingsRules::valid(tmp, on) || SettingsRules::valid(tmp, off)) {
            s = tmp;
            return true;
        }
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

uint16_t bumpBootCount() {
    uint16_t n = (uint16_t)(s_prefs.getUShort("boot", 0) + 1);
    s_prefs.putUShort("boot", n);
    return n;
}

}  // namespace Store
