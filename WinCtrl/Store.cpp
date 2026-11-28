#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
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

// ---------------- 10분 요약 ----------------
static bool     s_fsOk = false;
static uint32_t s_head = 0, s_count = 0;
static const char* ST_BIN = "/st.bin";
static const char* ST_IDX = "/st.idx";

bool statsBegin() {
    s_fsOk = LittleFS.begin(true);          // 처음이면 포맷
    if (!s_fsOk) { Serial.println("[stat] flash FS mount FAILED: summaries not saved"); return false; }
    s_head = s_count = 0;
    File f = LittleFS.open(ST_IDX, "r");
    if (f) {
        uint32_t v[2];
        if (f.read((uint8_t*)v, sizeof(v)) == sizeof(v) && v[0] < STATS_CAP && v[1] <= STATS_CAP) { s_head = v[0]; s_count = v[1]; }
        f.close();
    }
    Serial.printf("[stat] flash summaries %lu (cap %lu, 10 min each), FS used %lu/%lu bytes\n",
                  (unsigned long)s_count, (unsigned long)STATS_CAP,
                  (unsigned long)LittleFS.usedBytes(), (unsigned long)LittleFS.totalBytes());
    return true;
}

bool statsAppend(const StatSummary& s) {
    if (!s_fsOk) return false;
    File f = LittleFS.exists(ST_BIN) ? LittleFS.open(ST_BIN, "r+") : LittleFS.open(ST_BIN, "w");
    if (!f) return false;
    bool ok = f.seek(s_head * sizeof(StatSummary)) && f.write((const uint8_t*)&s, sizeof(s)) == sizeof(s);
    f.close();
    if (!ok) return false;
    s_head = (s_head + 1) % STATS_CAP;
    if (s_count < STATS_CAP) s_count++;
    File x = LittleFS.open(ST_IDX, "w");
    if (!x) return false;
    uint32_t v[2] = { s_head, s_count };
    ok = x.write((const uint8_t*)v, sizeof(v)) == sizeof(v);
    x.close();
    return ok;
}

uint32_t statsCount() { return s_count; }

void statsEach(uint32_t n, void (*fn)(const StatSummary&, void*), void* ctx) {
    if (!s_fsOk || s_count == 0) return;
    if (n > s_count) n = s_count;
    File f = LittleFS.open(ST_BIN, "r");
    if (!f) return;
    uint32_t start = (s_head + STATS_CAP - n) % STATS_CAP;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (start + i) % STATS_CAP;
        StatSummary s;
        if (!f.seek(idx * sizeof(StatSummary)) || f.read((uint8_t*)&s, sizeof(s)) != sizeof(s)) break;
        fn(s, ctx);
    }
    f.close();
}

}  // namespace Store
