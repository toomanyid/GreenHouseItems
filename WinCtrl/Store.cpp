#include <Arduino.h>
#include <Preferences.h>
#include "Store.h"
#include "Settings.h"

static Preferences s_prefs;
static const char* NS = "winctrl";

// 설정 버전 1의 저장 모양. 상한 리밋이 초(cs)였고 LINK가 없었다. 읽어서 버전 2로 옮긴다.
struct WindowSettingsV1 {
    cs_t     upTime, downTime, overlap, backoff, limit, homeTime, totalUp;
    int32_t  iMaxUp, iMaxDn;
    uint8_t  ovlRate;
    int16_t  maxRunS;
    uint8_t  rehome;
    bool     homeSensAuto;
};
struct SettingsV1 {
    uint16_t         version;
    WindowSettingsV1 win[2];
    TempSettings     temp;
    bool             clickSound;
    bool             alarmSound;
};

static void fromV1(const SettingsV1& o, Settings& s) {
    SettingsRules::defaults(s);
    for (uint8_t i = 0; i < 2; i++) {
        const WindowSettingsV1& a = o.win[i];
        WindowSettings& w = s.win[i];
        w.upTime = a.upTime; w.downTime = a.downTime; w.overlap = a.overlap; w.backoff = a.backoff;
        w.homeTime = a.homeTime; w.totalUp = a.totalUp; w.iMaxUp = a.iMaxUp; w.iMaxDn = a.iMaxDn;
        w.ovlRate = a.ovlRate; w.maxRunS = a.maxRunS; w.rehome = a.rehome; w.homeSensAuto = a.homeSensAuto;
        w.link = true;
        // 초로 넣은 리밋을 백분율로. 센서 유무를 아직 모르므로 설정대로 본다
        cs_t top = SettingsRules::maxPos(w, w.homeSensAuto);
        if (a.limit < 0 || top <= 0 || a.limit >= top) w.limitPct = PCT_MAX;
        else w.limitPct = SettingsRules::pctFromPos(a.limit, top);
    }
    s.temp = o.temp;
    s.clickSound = o.clickSound;
    s.alarmSound = o.alarmSound;
}

namespace Store {

void begin() {
    s_prefs.begin(NS, false);
}

bool loadSettings(Settings& s) {
    Settings tmp;
    size_t n = s_prefs.getBytesLength("set");
    bool got = false;
    if (n == sizeof(Settings) && s_prefs.getBytes("set", &tmp, sizeof(tmp)) == sizeof(tmp)
        && tmp.version == SETTINGS_VERSION) {
        got = true;
    }
    if (!got && n == sizeof(SettingsV1)) {
        SettingsV1 old;
        if (s_prefs.getBytes("set", &old, sizeof(old)) == sizeof(old) && old.version == 1) {
            fromV1(old, tmp);
            got = true;
        }
    }
    if (got) {
        // 센서 감지 전이므로 검사는 느슨한 쪽(센서 없음/있음 둘 중 하나라도 맞으면)으로 본다
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
