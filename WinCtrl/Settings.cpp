#include "Settings.h"
#include "config.h"

namespace SettingsRules {

void defaultsWindow(WindowSettings& w) {
    w.upTime       = CS_EMPTY;   // 비어 있으면 모델 기본값 (fillModelDefaults)
    w.downTime     = CS_EMPTY;
    w.homeTime     = CS_EMPTY;
    w.backoff      = 150;        // 1.5초
    w.sensorRun    = CS_EMPTY;
    w.limitPct     = PCT_MAX;    // 99.9 = 리밋 없음
    w.iMaxUp       = -1;
    w.iMaxDn       = -1;
    w.maxRunS      = -1;         // 상승 시간의 1.2배
    w.homeSensAuto = false;      // 원점 센서는 기본으로 쓰지 않는다 (2026-11-22)
    w.link         = true;
}

void defaultsTemp(TempSettings& t) {
    t.sensorAuto = true;
    t.setT       = 280;
    t.deadband   = 5;
    t.periodS    = 60;
    t.step       = 500;
    t.stepAdd    = 250;
    t.stepMax    = 3000;
    t.slopeMin   = 5;
    t.aheadMin   = 3;
    t.t4         = 0;
    t.t20        = 500;
    t.offset     = 0;
}

void defaults(Settings& s) {
    s.version = SETTINGS_VERSION;
    defaultsWindow(s.win[0]);
    defaultsWindow(s.win[1]);
    defaultsTemp(s.temp);
    s.clickSound = true;
    s.alarmSound = true;
}

bool fillModelDefaults(WindowSettings& w, Model m) {
    bool w300 = m == Model::W300;
    bool used = false;
    cs_t travel = w300 ? DEFAULT_TRAVEL_CS_300W : DEFAULT_TRAVEL_CS_50W;
    int32_t imax = w300 ? DEFAULT_IMAX_MA_300W : DEFAULT_IMAX_MA_50W;
    if (w.upTime < 0)   { w.upTime = travel; used = true; }
    if (w.downTime < 0) { w.downTime = travel; used = true; }
    if (w.homeTime < 0) { w.homeTime = w300 ? DEFAULT_HOME_CS_300W : DEFAULT_HOME_CS_50W; used = true; }
    if (w.iMaxUp < 0)   { w.iMaxUp = imax; used = true; }
    if (w.iMaxDn < 0)   { w.iMaxDn = imax; used = true; }
    return used;
}

static bool inRangeOrEmpty(cs_t v, cs_t lo, cs_t hi) {
    return v == CS_EMPTY || (v >= lo && v <= hi);
}

cs_t sensorPoint(const WindowSettings& w) {
    cs_t home = w.homeTime > 0 ? w.homeTime : 0;
    if (w.backoff <= 0) return home;
    if (w.upTime <= 0 || w.downTime <= 0) return home + w.backoff;
    return home + (cs_t)((int64_t)w.backoff * w.upTime / w.downTime);   // 되내림은 하강이므로 상승 기준으로
}

bool validWindow(const WindowSettings& w) {
    if (!inRangeOrEmpty(w.upTime, RANGE_TIME_MIN, RANGE_TIME_MAX)) return false;
    if (!inRangeOrEmpty(w.downTime, RANGE_TIME_MIN, RANGE_TIME_MAX)) return false;
    if (!inRangeOrEmpty(w.homeTime, 0, RANGE_HOME_MAX)) return false;
    if (w.backoff < 0 || w.backoff > RANGE_BACKOFF_MAX) return false;
    if (w.iMaxUp != -1 && (w.iMaxUp < RANGE_IMAX_MIN || w.iMaxUp > RANGE_IMAX_MAX)) return false;
    if (w.iMaxDn != -1 && (w.iMaxDn < RANGE_IMAX_MIN || w.iMaxDn > RANGE_IMAX_MAX)) return false;
    if (w.maxRunS != -1 && (w.maxRunS < RANGE_MAXRUN_MIN || w.maxRunS > RANGE_MAXRUN_MAX)) return false;
    if (w.limitPct < PCT_MIN || w.limitPct > PCT_MAX) return false;
    if (w.homeTime >= 0 && w.upTime >= 0 && w.homeTime >= w.upTime) return false;
    if (w.maxRunS != -1 && w.upTime >= 0 && (cs_t)w.maxRunS * 100 < w.upTime) return false;
    return true;
}

bool validTemp(const TempSettings& t) {
    if (t.setT < 0 || t.setT > 500) return false;
    if (t.deadband < 5 || t.deadband > 50) return false;
    if (t.periodS < 10 || t.periodS > 600) return false;
    if (t.step < 50 || t.step > 6000) return false;
    if (t.stepAdd < 0 || t.stepAdd > 3000) return false;
    if (t.stepMax < 50 || t.stepMax > 12000) return false;
    if (t.stepMax < t.step) return false;
    if (t.slopeMin < 1 || t.slopeMin > 30) return false;
    if (t.aheadMin > 15) return false;
    if (t.t4 < -500 || t.t4 > 500) return false;
    if (t.t20 < 0 || t.t20 > 1500) return false;
    if (t.t20 <= t.t4) return false;
    if (t.offset < -50 || t.offset > 50) return false;
    return true;
}

bool valid(const Settings& s) {
    return validWindow(s.win[0]) && validWindow(s.win[1]) && validTemp(s.temp);
}

bool calInRange(const CalStatus& c) {
    if (c.upTime < RANGE_TIME_MIN || c.upTime > RANGE_TIME_MAX) return false;
    if (c.downTime < RANGE_TIME_MIN || c.downTime > RANGE_TIME_MAX) return false;
    if (c.sensorMode && (c.sensorRun < 0 || c.sensorRun >= c.upTime)) return false;
    if (c.iMaxUp < RANGE_IMAX_MIN || c.iMaxUp > RANGE_IMAX_MAX) return false;
    if (c.iMaxDn < RANGE_IMAX_MIN || c.iMaxDn > RANGE_IMAX_MAX) return false;
    return true;
}

pct_t pctFromPos(cs_t pos, cs_t home, cs_t top) {
    cs_t span = top - home;
    if (span <= 0 || pos <= home) return PCT_MIN;
    int64_t p = PCT_MIN + ((int64_t)(pos - home) * (PCT_MAX - PCT_MIN) * 2 + span) / (2 * (int64_t)span);
    if (p > PCT_MAX) p = PCT_MAX;
    return (pct_t)p;
}

cs_t posFromPct(pct_t pct, cs_t home, cs_t top) {
    cs_t span = top - home;
    if (pct <= PCT_MIN || span <= 0) return home;
    if (pct >= PCT_MAX) return top;
    return home + (cs_t)(((int64_t)(pct - PCT_MIN) * span * 2 + (PCT_MAX - PCT_MIN)) / (2 * (PCT_MAX - PCT_MIN)));
}

cs_t limitPos(const WindowSettings& w, cs_t home, cs_t top) {
    if (w.limitPct >= PCT_MAX) return top;
    return posFromPct(w.limitPct, home, top);
}

void recomputeHomeTime(WindowSettings& w) {
    if (w.sensorRun < 0 || w.upTime <= 0 || w.downTime <= 0) return;
    cs_t back = (cs_t)((int64_t)w.backoff * w.upTime / w.downTime);
    cs_t h = w.sensorRun - back;
    if (h < 0) h = 0;
    if (h > RANGE_HOME_MAX || h >= w.upTime) return;
    w.homeTime = h;
}

}  // namespace SettingsRules
