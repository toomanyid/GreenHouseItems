#include "Settings.h"

namespace SettingsRules {

void defaultsWindow(WindowSettings& w) {
    w.upTime       = CS_EMPTY;
    w.downTime     = CS_EMPTY;
    w.overlap      = CS_EMPTY;
    w.backoff      = 150;        // 1.5초
    w.limit        = CS_EMPTY;   // 리밋 없음
    w.homeTime     = CS_EMPTY;
    w.totalUp      = CS_EMPTY;
    w.iMaxUp       = -1;
    w.iMaxDn       = -1;
    w.ovlRate      = 13;
    w.maxRunS      = -1;         // 전체 상승 시간의 1.2배
    w.rehome       = 20;
    w.homeSensAuto = true;
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

static bool inRangeOrEmpty(cs_t v, cs_t lo, cs_t hi) {
    return v == CS_EMPTY || (v >= lo && v <= hi);
}

cs_t sensorPoint(const WindowSettings& w) {
    if (w.backoff <= 0) return 0;
    if (w.upTime <= 0 || w.downTime <= 0) return w.backoff;
    return (cs_t)((int64_t)w.backoff * w.upTime / w.downTime);
}

cs_t maxPos(const WindowSettings& w, bool sensorActive) {
    if (w.upTime < 0) return 0;
    if (sensorActive) return sensorPoint(w) + w.upTime;
    if (w.totalUp >= 0 && w.homeTime >= 0 && w.totalUp > w.homeTime) return w.totalUp - w.homeTime;
    return w.upTime;
}

bool validWindow(const WindowSettings& w, bool sensorActive) {
    if (!inRangeOrEmpty(w.upTime, RANGE_TIME_MIN, RANGE_TIME_MAX)) return false;
    if (!inRangeOrEmpty(w.downTime, RANGE_TIME_MIN, RANGE_TIME_MAX)) return false;
    if (!inRangeOrEmpty(w.overlap, 0, RANGE_OVERLAP_MAX)) return false;
    if (w.backoff < 0 || w.backoff > RANGE_BACKOFF_MAX) return false;
    if (!inRangeOrEmpty(w.homeTime, 0, RANGE_HOME_MAX)) return false;
    if (w.iMaxUp != -1 && (w.iMaxUp < RANGE_IMAX_MIN || w.iMaxUp > RANGE_IMAX_MAX)) return false;
    if (w.iMaxDn != -1 && (w.iMaxDn < RANGE_IMAX_MIN || w.iMaxDn > RANGE_IMAX_MAX)) return false;
    if (w.ovlRate < RANGE_OVL_MIN || w.ovlRate > RANGE_OVL_MAX) return false;
    if (w.maxRunS != -1 && (w.maxRunS < RANGE_MAXRUN_MIN || w.maxRunS > RANGE_MAXRUN_MAX)) return false;
    if (w.rehome > RANGE_REHOME_MAX) return false;

    if (w.limit != CS_EMPTY) {
        if (w.limit < 0 || w.upTime < 0) return false;
        if (w.limit > maxPos(w, sensorActive)) return false;
    }
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

bool valid(const Settings& s, const bool sensorActive[2]) {
    return validWindow(s.win[0], sensorActive[0])
        && validWindow(s.win[1], sensorActive[1])
        && validTemp(s.temp);
}

bool calInRange(const CalStatus& c) {
    if (c.effUp < RANGE_TIME_MIN || c.effUp > RANGE_TIME_MAX) return false;
    if (c.effDown < RANGE_TIME_MIN || c.effDown > RANGE_TIME_MAX) return false;
    if (c.sensorMode && (c.overlap < 0 || c.overlap > RANGE_OVERLAP_MAX)) return false;
    if (c.iMaxUp < RANGE_IMAX_MIN || c.iMaxUp > RANGE_IMAX_MAX) return false;
    if (c.iMaxDn < RANGE_IMAX_MIN || c.iMaxDn > RANGE_IMAX_MAX) return false;
    return true;
}

void recomputeHomeTime(WindowSettings& w) {
    if (w.totalUp < 0 || w.upTime < 0) return;
    cs_t h = w.totalUp - w.upTime - sensorPoint(w);
    if (h < 0) h = 0;
    if (h > RANGE_HOME_MAX) return;
    w.homeTime = h;
}

}  // namespace SettingsRules
