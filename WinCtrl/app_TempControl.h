#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  3층: 온도제어 (스펙 9절, 단계식 개폐)
//  판단 주기마다 예측 온도로 두 창을 한 단계씩 여닫는 잡을 만든다.
// ============================================================

class TempControl {
public:
    struct WinView {
        bool     eligible;   // 고장, 비상정지, 측정 중이 아니고 이동 시간 설정이 있음
        PosState posState;
        cs_t     pos;
        cs_t     limit;
    };

    void begin(uint32_t now);
    void setSettings(const TempSettings& t) { _t = t; }

    // 4~20mA 전류(µA)를 온도(0.1도)로
    static int16_t toTemp(int32_t ua, const TempSettings& t);

    // 매 주기. valid가 false면 끊김으로 보고 멈춘다. 만든 잡 수를 돌려준다 (최대 2)
    uint8_t tick(uint32_t now, bool valid, int16_t temp, bool paused, const WinView v[2], Job jobs[2]);

    // 모터에 직접 목표를 넣었거나 정지, 고장, 센서 끊김이 있으면 처음 크기로
    void resetStep() { _streak = 0; _lastDir = 0; }

    int16_t  predicted() const { return _pred; }
    int16_t  slope() const     { return _slope; }
    cs_t     stepSize() const;

private:
    void    record(int16_t temp);
    int16_t computeSlope() const;

    TempSettings _t = {};
    int16_t  _log[TEMP_LOG_SIZE] = {};
    uint16_t _logHead = 0;
    uint16_t _logCount = 0;
    uint32_t _lastLogMs = 0;
    uint32_t _lastDecisionMs = 0;
    bool     _wasValid = false;
    bool     _wasPaused = false;
    int16_t  _pred = 0;
    int16_t  _slope = 0;
    uint8_t  _streak = 0;      // 같은 방향으로 연속해서 움직인 횟수
    int8_t   _lastDir = 0;
};
