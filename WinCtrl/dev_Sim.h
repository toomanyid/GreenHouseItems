#pragma once
#include <stdint.h>
#include "config.h"

// ============================================================
//  1층: UI 시험 빌드(-DWINCTRL_UISIM) 전용 가짜 개폐기.
//  LCD와 엔코더만 단 보드에서 화면을 끝까지 돌려 보려고 쓴다.
//  모터 핀은 쓰지 않고, 구동 명령대로 창 위치를 셈해서
//  전류(끝에 닿으면 0, 리밋 스위치처럼)와 원점 센서 값을 만든다.
//  제어 태스크에서만 쓴다. 현장 설치 빌드에는 들어가지 않는다.
//  책상 시험 빌드(-DWINCTRL_BENCH)는 여기서 SIM_24V_MV만 쓴다 (dev_Relay.cpp).
// ============================================================
const cs_t    SIM_TRAVEL_CS = 3000;    // 끝에서 끝까지 30초 (전속)
const cs_t    SIM_SENSOR_CS = 300;     // 닫힘 끝에서 3초 안이면 원점 센서 ON
const cs_t    SIM_START_CS  = 1500;    // 전원 켤 때 위치 (가운데)
const int32_t SIM_TEMP_UA   = 12000;   // 12mA: 기본 설정(4mA 0.0도, 20mA 50.0도)에서 25.0도
const int32_t SIM_24V_MV    = 24000;

class SimWindow {
public:
    // 지난 시간만큼 움직인다. 위치 단위는 cs의 1/1000, 전속(1000)이면 10ms에 1cs
    void step(int8_t dir, uint16_t duty, uint32_t dtMs) {
        int32_t d = (int32_t)duty * (int32_t)dtMs / 10;
        if (dir > 0) { _u += d; if (_u > TOP) _u = TOP; }
        if (dir < 0) { _u -= d; if (_u < 0) _u = 0; }
    }
    // 정상 운전 전류. 끝에 닿은 쪽으로는 흐르지 않는다
    int32_t currentMa(int8_t dir, uint16_t duty) const {
        if (dir == 0 || duty == 0) return 0;
        if (dir > 0 && _u >= TOP) return 0;
        if (dir < 0 && _u <= 0) return 0;
        return 2000 * (int32_t)(duty < 200 ? 200 : duty) / 1000 + 400;
    }
    bool sensorOn() const { return _u < SIM_SENSOR_CS * 1000; }
    cs_t pos() const      { return _u / 1000; }

private:
    static const int32_t TOP = SIM_TRAVEL_CS * 1000;
    int32_t _u = SIM_START_CS * 1000;
};

// 창 두 개 (0: 모터1, 1: 모터2)
inline SimWindow& simWindow(uint8_t i) {
    static SimWindow w[2];
    return w[i ? 1 : 0];
}
