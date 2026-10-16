#include "ctl_Safety.h"

// ============================================================
//  CurrentMonitor
// ============================================================
void CurrentMonitor::start() {
    _running = true;
    _count = 0;
    _filled = 0;
    _head = 0;
    _sum = 0;
    _everFlowed = false;
    _lowRun = 0;
    _moveMax = 0;
    _settledMax = 0;
    _settledSum = 0;
    _settledN = 0;
}

void CurrentMonitor::sample(int32_t ma) {
    if (!_running) return;

    // 0.3초 전 값이 고리에서 빠져나간다. 기동 후 1초가 지난 값만 측정 결과에 넣는다.
    // 그래서 전류가 끊기는 마지막 0.3초는 자연히 빠진다.
    if (_filled >= WINDOW) {
        uint32_t oldIndex = _count - WINDOW;
        if (oldIndex >= STARTUP_SAMPLES) {
            _settledSum += _samples[_head];
            _settledN++;
        }
        // 평균 구간 전체가 기동 1초 뒤에 있는 것만 최대값 후보
        if (oldIndex >= STARTUP_SAMPLES + WINDOW - 1 && _avgs[_head] > _settledMax) {
            _settledMax = _avgs[_head];
        }
        _sum -= _samples[_head];
    }

    _samples[_head] = ma;
    _sum += ma;
    if (_filled < WINDOW) _filled++;
    int32_t avg = (int32_t)(_sum / _filled);
    _avgs[_head] = (_filled >= WINDOW) ? avg : 0;
    _head = (uint8_t)((_head + 1) % WINDOW);
    _count++;

    if (ma >= NO_CURRENT_MA) {
        _everFlowed = true;
        _lowRun = 0;
    } else if (_everFlowed && _lowRun < LOW_RUN_SAMPLES) {
        _lowRun++;
    }

    if (pastStartup() && windowFull() && avg > _moveMax) _moveMax = avg;
}

int32_t CurrentMonitor::average() const {
    return _filled ? (int32_t)(_sum / _filled) : 0;
}

// ============================================================
//  RelayControl
// ============================================================
void RelayControl::begin(uint32_t now) {
    _phase = Phase::BOOT_CHECK;
    _phaseMs = now;
    _fetA = _fetB = _ready = _fault = false;
    _decayCheck = false;
}

void RelayControl::tick(uint32_t now, int32_t relayMv, bool wantOn) {
    if (_fault) return;
    uint32_t inPhase = now - _phaseMs;

    switch (_phase) {
    // 전원 켤 때: 릴레이가 떨어진 채 전압이 없어야 하고, A만 켜거나 B만 켜도 전압이 없어야 한다
    case Phase::BOOT_CHECK:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            if (relayMv >= RELAY_OFF_MV) { setFault(); return; }
            _fetA = true; _fetB = false;
            _phase = Phase::TEST_A; _phaseMs = now;
        }
        break;
    case Phase::TEST_A:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            if (relayMv >= RELAY_OFF_MV) { setFault(); return; }   // B가 붙어 있음
            _fetA = false;
            _phase = Phase::GAP; _phaseMs = now;
        }
        break;
    case Phase::GAP:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            _fetB = true;
            _phase = Phase::TEST_B; _phaseMs = now;
        }
        break;
    case Phase::TEST_B:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            if (relayMv >= RELAY_OFF_MV) { setFault(); return; }   // A가 붙어 있음
            _fetB = false;
            _phase = Phase::OFF; _phaseMs = now;
        }
        break;

    case Phase::OFF:
        // 뗀 뒤 일정 시간이 지나도 전압이 남으면 접점이 붙은 것
        if (_decayCheck && now - _offMs >= RELAY_DECAY_MS) {
            _decayCheck = false;
            if (relayMv >= RELAY_OFF_MV) { setFault(); return; }
        }
        if (wantOn) {
            _decayCheck = false;
            _fetB = true;                         // 붙일 때는 B, A 순서
            _phase = Phase::ENGAGE_B; _phaseMs = now;
        }
        break;
    case Phase::ENGAGE_B:
        _fetA = true;
        _phase = Phase::ENGAGE_A; _phaseMs = now;
        break;
    case Phase::ENGAGE_A:
        if (inPhase >= RELAY_SETTLE_MS) {
            if (relayMv < RELAY_ON_MV) { setFault(); return; }      // 퓨즈나 접점 불량
            _ready = true;
            _phase = Phase::ON; _phaseMs = now;
        }
        break;
    case Phase::ON:
        if (relayMv < RELAY_OFF_MV) { setFault(); return; }
        if (!wantOn) { _phase = Phase::RELEASE_WAIT; _phaseMs = now; _ready = false; }
        break;
    case Phase::RELEASE_WAIT:
        // 끊은 뒤 잠깐 기다려 전류가 없을 때 뗀다
        if (wantOn) { _ready = true; _phase = Phase::ON; _phaseMs = now; break; }
        if (inPhase >= RELAY_RELEASE_DELAY_MS) {
            _fetA = _fetB = false;
            _offMs = now;
            _decayCheck = true;
            _phase = Phase::OFF; _phaseMs = now;
        }
        break;
    }
}
