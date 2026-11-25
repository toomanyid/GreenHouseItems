#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  설정값의 기본값과 범위 검사 (스펙 8절). 범위 검사는 여기 한 곳에만 둔다.
//  플래시 저장은 SettingsStore에서 한다.
// ============================================================

const uint16_t SETTINGS_VERSION = 3;   // 3: 위치를 bottom 기준으로, 겹침/배율/재원점 없앰 (2026-11-22). 예전 것은 읽지 않음

// 범위 (내부 단위)
const cs_t    RANGE_TIME_MIN    = 500;     // 상승/하강 5.0~900.0초
const cs_t    RANGE_TIME_MAX    = 90000;
const cs_t    RANGE_BACKOFF_MAX = 1000;    // 되내림 0.0~10.0초
const cs_t    RANGE_HOME_MAX    = 12000;   // 원점 시간 0.0~120.0초
const int32_t RANGE_IMAX_MIN    = 300;     // 최대 운전 전류 300~30000mA
const int32_t RANGE_IMAX_MAX    = 30000;
const int16_t RANGE_MAXRUN_MIN  = 10;      // 최대 구동 시간 10~1200초
const int16_t RANGE_MAXRUN_MAX  = 1200;

namespace SettingsRules {
    void defaults(Settings& s);
    void defaultsWindow(WindowSettings& w);
    void defaultsTemp(TempSettings& t);

    // 비어 있는 모터 정보(상승/하강/원점 시간, 최대 운전 전류)를 모델별 기본값으로 채운다.
    // 기본값을 하나라도 쓰면 true (켤 때 경고)
    bool fillModelDefaults(WindowSettings& w, Model m);

    // 저장해도 되는 값인지. 원점 시간이 상승 시간보다 길거나 최대 구동 시간이 상승 시간보다 짧으면 false
    bool validWindow(const WindowSettings& w);
    bool validTemp(const TempSettings& t);
    bool valid(const Settings& s);

    // 측정 결과가 허용 범위 안인지
    bool calInRange(const CalStatus& c);

    // 센서를 켠 창: 원점 시간 = bottom → 센서 지점 − 되내림 거리 (스펙 10절). 잰 값이 없으면 그대로 둔다
    void recomputeHomeTime(WindowSettings& w);

    // 위치 (bottom = 0, 스펙 3절). 비어 있는 항목은 부르는 쪽이 기본값을 채워 넘긴다
    cs_t sensorPoint(const WindowSettings& w);   // home + 되내림 거리(상승 기준)

    // 위치 ↔ 백분율. 백분율 = 1.0 + (pos − home) × 98.9 ÷ (top − home), 0.1 단위로 반올림.
    // pctFromPos는 home~top을 1.0~99.9로, bottom~home을 0.0~1.0으로 돌려준다 (2026-11-25).
    // 0.0은 CLOSE와 같은 값이다. 닫힘 상태는 부르는 쪽이 따로 CLOSE로 본다
    pct_t pctFromPos(cs_t pos, cs_t home, cs_t top);
    cs_t  posFromPct(pct_t pct, cs_t home, cs_t top);
    // 상한 리밋 위치. 리밋 없음이면 top
    cs_t  limitPos(const WindowSettings& w, cs_t home, cs_t top);
}
