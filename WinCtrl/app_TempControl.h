#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  3층: 온도제어 (스펙 9절, 단계식 개폐)
//  판단 주기마다 예측 온도로 두 창을 한 단계씩 여닫는 잡을 만든다.
//  사람이 맞추지 않아도 스스로 따라가는 것 (2026-11-28):
//   - 창을 움직인 뒤 온도 흐름(기울기의 방향)이 그 움직임이 노린 쪽으로 바뀌기까지의 시간(반응 시간)을 잰다.
//     온실의 반응 속도는 바람에 따라 바뀐다. 최근 측정으로 판단 주기를 반응 시간 × 1.2로 줄인다
//     (바람이 불면 짧아짐). 설정한 판단 주기보다 길어지지 않는다.
//   - 우리가 움직인 효과로 흐름이 거꾸로 바뀌면 더 움직이지 않고 판단 주기를 처음부터 다시 센다
//     (효과가 나타나는 중에 더 열지 않게).
//   - 해, 구름처럼 열원이 바뀌어 생긴 역전은 아직 다루지 않는다 (따로 정한 뒤 넣는다).
// ============================================================

class TempControl {
public:
    struct WinView {
        bool     eligible;   // LINK가 켜져 있고, 고장, 비상정지, 측정 중이 아니고 이동 시간 설정이 있음
        PosState posState;
        cs_t     pos;        // bottom = 0
        cs_t     home;       // home 위치 (닫는 이동은 여기서 멈춘다)
        cs_t     limit;
    };

    void begin(uint32_t now);
    void setSettings(const TempSettings& t) { _t = t; }

    // 4~20mA 전류(µA)를 온도(0.1도)로
    static int16_t toTemp(int32_t ua, const TempSettings& t);

    // 매 주기. valid가 false면 끊김으로 보고 멈춘다. 만든 잡 수를 돌려준다 (최대 2)
    uint8_t tick(uint32_t now, bool valid, int16_t temp, bool paused, const WinView v[2], Job jobs[2]);

    // 모터에 직접 목표를 넣었거나 정지, 고장, 센서 끊김이 있으면 처음 크기로.
    // 그 움직임은 온도제어가 한 것이 아니므로 반응 시간 측정도 버린다
    void resetStep() { _streak = 0; _lastDir = 0; _pendMove = false; _lastMoveDir = 0; _settling = false; }

    // 설정 온도가 바뀌면 판단 주기를 기다리지 않고 다음 tick에서 판단한다 (2026-11-26).
    // 멈춤(메뉴, 수동) 중이면 풀린 뒤 첫 tick에서 판단한다
    void decideNow() { _decideNow = true; }

    int16_t  predicted() const { return _pred; }
    int16_t  slope() const     { return _slope; }
    // 다음 판단까지 남은 초 (마지막 tick 시각 기준, 화면 표시용)
    uint16_t nextDecisionS() const {
        uint32_t p = periodMs(), e = _nowMs - _lastDecisionMs;
        return e >= p ? 0 : (uint16_t)((p - e + 999) / 1000);
    }
    // 지금 쓰는 판단 주기: 반응 시간이 있으면 × 1.2 (최소 30초, 설정값보다 길지 않음)
    uint32_t periodMs() const;
    // 반응 시간 추정 (초, 0 = 아직 모름), 마지막 측정, 측정 횟수
    uint16_t responseS() const;
    uint16_t lastResponseS() const { return _respLastS; }
    // 효과의 세기: 1cs 움직일 때 기울기가 바뀌는 양 (분당 0.0001도, 0 = 아직 모름). 바람이 세면 커진다
    int32_t  gain() const;
    uint16_t gainCount() const { return _gainCount; }
    bool     settling() const { return _settling; }
    uint16_t responseCount() const { return _respCount; }
    // 온도 흐름: +1 오름, -1 내림, 0 아직 모름. 역전 횟수와 마지막 역전에서 한 일
    int8_t   trend() const { return _trendDir; }
    uint16_t reversalCount() const { return _revCount; }
    bool     lastReversalHeld() const { return _revHeld; }   // 참: 우리 효과라 기다림, 거짓: 바깥 변화(지금은 아무것도 안 함)
    cs_t     stepSize() const;

private:
    void    record(int16_t temp);
    int16_t computeSlope() const;
    void    updateTrend(uint32_t now, bool paused);
    void    addResponse(uint32_t ms, uint32_t now);
    void    addGain(int32_t g, uint32_t now);
    void    checkEffect(uint32_t now);

    TempSettings _t = {};
    int16_t  _log[TEMP_LOG_SIZE] = {};
    uint16_t _logHead = 0;
    uint16_t _logCount = 0;
    uint32_t _lastLogMs = 0;
    uint32_t _lastDecisionMs = 0;
    uint32_t _nowMs = 0;
    bool     _wasValid = false;
    bool     _wasPaused = false;
    int16_t  _pred = 0;
    int16_t  _slope = 0;
    uint8_t  _streak = 0;      // 같은 방향으로 연속해서 움직인 횟수
    int8_t   _lastDir = 0;
    bool     _decideNow = false;

    // 온도 흐름
    int8_t   _trendDir = 0;       // 정해진 흐름
    int8_t   _candDir = 0;        // 지금 기울기의 방향 (이어지는지 보는 중)
    uint32_t _candSince = 0;
    uint16_t _revCount = 0;
    bool     _revHeld = false;
    // 마지막으로 온도제어가 움직인 것
    uint32_t _lastMoveMs = 0;
    int8_t   _lastMoveDir = 0;    // +1 엶, -1 닫음
    bool     _pendMove = false;   // 반응을 기다리는 중
    uint32_t _pendMs = 0;
    int8_t   _pendDir = 0;
    // 반응 시간 측정 (최근 것부터)
    uint32_t _respMs[TEMP_RESP_KEEP] = {};
    uint32_t _respAt[TEMP_RESP_KEEP] = {};
    uint8_t  _respN = 0;
    uint16_t _respLastS = 0;
    uint16_t _respCount = 0;
    // 효과 기다리기와 효과의 세기
    bool     _settling = false;
    int8_t   _setDir = 0;
    int16_t  _setSlope = 0;       // 움직일 때의 기울기
    int32_t  _setPeak = 0;        // 노린 쪽으로 바뀐 기울기의 가장 큰 값
    cs_t     _setSize = 0;        // 움직인 크기
    uint32_t _setMs = 0;
    int32_t  _gain[TEMP_GAIN_KEEP] = {};
    uint32_t _gainAt[TEMP_GAIN_KEEP] = {};
    uint8_t  _gainN = 0;
    uint16_t _gainCount = 0;
};
