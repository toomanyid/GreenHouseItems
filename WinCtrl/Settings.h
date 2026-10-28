#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  설정값의 기본값과 범위 검사 (스펙 8절). 범위 검사는 여기 한 곳에만 둔다.
//  플래시 저장은 SettingsStore에서 한다.
// ============================================================

const uint16_t SETTINGS_VERSION = 2;   // 2: 상한 리밋을 백분율로, LINK 추가 (1은 Store가 옮겨 읽음)

// 범위 (내부 단위)
const cs_t    RANGE_TIME_MIN    = 500;     // 유효 상승/하강 5.0~900.0초
const cs_t    RANGE_TIME_MAX    = 90000;
const cs_t    RANGE_OVERLAP_MAX = 12000;   // 겹침 하강 0.0~120.0초
const cs_t    RANGE_BACKOFF_MAX = 1000;    // 되내림 0.0~10.0초
const cs_t    RANGE_HOME_MAX    = 12000;   // 원점 시간 0.0~120.0초
const int32_t RANGE_IMAX_MIN    = 300;     // 최대 운전 전류 300~30000mA
const int32_t RANGE_IMAX_MAX    = 30000;
const uint8_t RANGE_OVL_MIN     = 11;      // 과부하 배율 1.1~2.0
const uint8_t RANGE_OVL_MAX     = 20;
const int16_t RANGE_MAXRUN_MIN  = 10;      // 최대 구동 시간 10~1200초
const int16_t RANGE_MAXRUN_MAX  = 1200;
const uint8_t RANGE_REHOME_MAX  = 100;

namespace SettingsRules {
    void defaults(Settings& s);
    void defaultsWindow(WindowSettings& w);
    void defaultsTemp(TempSettings& t);

    // 저장해도 되는 값인지. 상한 리밋이 최대 위치를 넘거나 최대 구동 시간이 상승 시간보다 짧으면 false
    bool validWindow(const WindowSettings& w, bool sensorActive);
    bool validTemp(const TempSettings& t);
    bool valid(const Settings& s, const bool sensorActive[2]);

    // 측정 결과가 허용 범위 안인지
    bool calInRange(const CalStatus& c);

    // 센서가 있는 창: 원점 시간 = (전체 상승 시간 − 유효 상승 시간) − 되내림 거리 (스펙 10절)
    void recomputeHomeTime(WindowSettings& w);

    // 창의 최대 위치 (ctl_Window와 같은 계산)
    cs_t sensorPoint(const WindowSettings& w);
    cs_t maxPos(const WindowSettings& w, bool sensorActive);

    // 위치 ↔ 백분율 (스펙 3절). 백분율 = 1.0 + pos × 98.9 ÷ 최대 위치, 0.1 단위로 반올림.
    // pctFromPos는 1.0~99.9를 돌려준다 (CLOSE는 부르는 쪽이 0.0으로 본다)
    pct_t pctFromPos(cs_t pos, cs_t maxPos);
    // 1.0~99.9 → pos = (백분율 − 1.0) × 최대 위치 ÷ 98.9
    cs_t  posFromPct(pct_t pct, cs_t maxPos);
    // 상한 리밋 위치. 리밋 없음이면 최대 위치
    cs_t  limitPos(const WindowSettings& w, cs_t maxPos);
}
