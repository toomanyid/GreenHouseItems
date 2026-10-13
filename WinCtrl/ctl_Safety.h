#pragma once
#include <stdint.h>
#include "config.h"
#include "Types.h"

// ============================================================
//  2층 안전 판정 도구 (하드웨어 없이 시험할 수 있게 순수 C++로 둔다)
// ============================================================

// 창 하나의 운전 전류 감시. 구동 중 10ms마다 sample()을 부른다.
class CurrentMonitor {
public:
    static const uint8_t WINDOW = OVERLOAD_WINDOW_MS / CONTROL_PERIOD_MS;          // 0.3초 = 30개
    static const uint16_t STARTUP_SAMPLES = START_IGNORE_MS / CONTROL_PERIOD_MS;    // 1초 = 100개

    void     start();                  // 구동 시작 (소프트 스타트 시작)
    void     sample(int32_t ma);
    void     reset() { start(); _running = false; }

    bool     running() const       { return _running; }
    bool     pastStartup() const   { return _count >= STARTUP_SAMPLES; }
    bool     windowFull() const    { return _filled >= WINDOW; }
    int32_t  average() const;                       // 최근 0.3초 평균
    bool     everFlowed() const    { return _everFlowed; }
    bool     stopped() const       { return _lowRun >= LOW_RUN_SAMPLES; }  // 흐르다가 0으로 떨어짐
    uint8_t  lowRun() const        { return _lowRun; }                      // 끊긴 뒤 지난 주기 수
    int32_t  moveMax() const       { return _moveMax; }                    // 기동 1초 뒤 0.3초 평균의 최대

    // 이동 시간 측정용: 기동 후 1초와 마지막 0.3초를 뺀 값
    int32_t  settledMax() const    { return _settledMax; }
    int32_t  settledAvg() const    { return _settledN ? (int32_t)(_settledSum / _settledN) : 0; }

private:
    static const uint8_t LOW_RUN_SAMPLES = 5;      // 50ms 이어지면 전류가 끊긴 것으로 봄

    bool     _running = false;
    uint32_t _count = 0;
    uint8_t  _filled = 0;
    uint8_t  _head = 0;
    int32_t  _samples[WINDOW] = {};
    int32_t  _avgs[WINDOW] = {};
    int64_t  _sum = 0;
    bool     _everFlowed = false;
    uint8_t  _lowRun = 0;
    int32_t  _moveMax = 0;
    int32_t  _settledMax = 0;
    int64_t  _settledSum = 0;
    uint32_t _settledN = 0;
};

// 주 릴레이: 전원 켤 때 자가 점검, 붙이고 떼는 순서, 붙음/불량 감시
class RelayControl {
public:
    void begin(uint32_t now);
    // relayMv: 릴레이 뒤 24V 쪽 전압 (mV). wantOn: 어느 창이든 구동하려 함
    void tick(uint32_t now, int32_t relayMv, bool wantOn);

    bool fetA() const        { return _fetA; }
    bool fetB() const        { return _fetB; }
    bool ready() const       { return _ready; }        // 붙고 0.1초 지나 구동해도 됨
    bool selfTestDone() const { return _phase >= Phase::OFF; }
    bool fault() const       { return _fault; }

private:
    enum class Phase : uint8_t { BOOT_CHECK, TEST_A, GAP, TEST_B, OFF, ENGAGE_B, ENGAGE_A, ON, RELEASE_WAIT };
    void  setFault() { _fault = true; _fetA = _fetB = false; _ready = false; }

    Phase    _phase = Phase::BOOT_CHECK;
    uint32_t _phaseMs = 0;
    uint32_t _offMs = 0;          // 뗀 시각 (붙음 검사용)
    bool     _decayCheck = false;
    bool     _fetA = false, _fetB = false, _ready = false, _fault = false;
};

// 단순한 시간 유지 필터: 같은 값이 holdMs 이어져야 바뀐다
class HoldFilter {
public:
    explicit HoldFilter(bool initial = false) : _value(initial), _cand(initial) {}
    bool update(bool raw, uint32_t now, uint32_t holdMs) {
        if (raw != _cand) { _cand = raw; _since = now; }
        if (_cand != _value && now - _since >= holdMs) _value = _cand;
        return _value;
    }
    bool value() const { return _value; }
    void set(bool v) { _value = _cand = v; }
private:
    bool     _value;
    bool     _cand;
    uint32_t _since = 0;
};

// 원점 센서 on/off 입력 하나를 "있음(ON)/없음(OFF)"으로 (스펙 6절, 10절)
//   같은 값이 0.1초 이어져야 바뀐다. 전원 켠 뒤 첫 판정 전에는 UNKNOWN.
//   단선은 OFF와 구분되지 않으므로 ctl_Window의 위치 확인으로 잡는다.
class SensorFilter {
public:
    void begin(uint32_t now) { _value = SensorState::UNKNOWN; _cand = SensorState::UNKNOWN; _since = now; }
    SensorState update(bool on, uint32_t now) {
        SensorState raw = on ? SensorState::PRESENT : SensorState::ABSENT;
        if (raw != _cand) { _cand = raw; _since = now; }
        if (_value != _cand && now - _since >= SENSOR_DEBOUNCE_MS) _value = _cand;
        return _value;
    }
    SensorState value() const { return _value; }
private:
    SensorState _value = SensorState::UNKNOWN;
    SensorState _cand = SensorState::UNKNOWN;
    uint32_t    _since = 0;
};
