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
    _fetA = _fetB = _ready = false;
    _warnPending = _warned = _lostWarned = false;
    _decayCheck = false;
}

void RelayControl::tick(uint32_t now, int32_t relayMv, bool wantOn, bool holdOk) {
    uint32_t inPhase = now - _phaseMs;

    switch (_phase) {
    // 전원 켤 때: MC가 떨어진 채 전압이 없어야 하고, A만 켜거나 B만 켜도 전압이 없어야 한다.
    // 어긋나면 경고만 내고 운전은 받는다 (2026-11-28)
    case Phase::BOOT_CHECK:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            if (relayMv >= RELAY_OFF_MV) {             // 이미 전압이 있음: 접점 융착 (A, B 시험은 뜻이 없다)
                warn();
                _phase = Phase::OFF; _phaseMs = now;
                break;
            }
            _fetA = true; _fetB = false;
            _phase = Phase::TEST_A; _phaseMs = now;
        }
        break;
    case Phase::TEST_A:
        if (inPhase >= RELAY_SELFTEST_STEP_MS) {
            if (relayMv >= RELAY_OFF_MV) warn();       // B가 붙어 있음
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
            if (relayMv >= RELAY_OFF_MV) warn();       // A가 붙어 있음
            _fetB = false;
            _phase = Phase::OFF; _phaseMs = now;
        }
        break;

    case Phase::OFF:
        // 뗀 뒤 일정 시간이 지나도 전압이 남으면 접점이 붙은 것
        if (_decayCheck && now - _offMs >= RELAY_DECAY_MS) {
            _decayCheck = false;
            if (relayMv >= RELAY_OFF_MV) warn();
        }
        if (wantOn) {
            // 붙이기 전부터 전압이 있으면 융착. 동작을 시도할 때마다 알린다 (뗀 직후 전압이 빠지는 중은 빼고)
            if (!_decayCheck && relayMv >= RELAY_OFF_MV) warn();
            _decayCheck = false;
            _lostWarned = false;
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
            // 붙였는데 전압이 없음(퓨즈, 코일, 접점 불량): 경고만 내고 구동은 그대로 시도한다.
            // 24V가 가지 않으면 창 쪽에서 무전류 경고로 그 구동이 끝난다
            if (relayMv < RELAY_ON_MV) { warn(); _lostWarned = true; }
            _ready = true;
            _phase = Phase::ON; _phaseMs = now;
        }
        break;
    case Phase::ON:
        if (relayMv < RELAY_OFF_MV && !_lostWarned) { warn(); _lostWarned = true; }
        if (!wantOn) { _phase = holdOk ? Phase::HOLD : Phase::RELEASE_WAIT; _phaseMs = now; _ready = false; }
        break;
    case Phase::HOLD:
        // 멈춘 뒤에도 잠시 붙여 둔다. 그 안에 다음 이동이 오면 다시 붙이지 않고 바로 구동한다
        if (wantOn) {
            _lostWarned = false;                  // 새 동작 시도
            _ready = true; _phase = Phase::ON; _phaseMs = now; break;
        }
        if (!holdOk || inPhase >= RELAY_HOLD_MS) { _phase = Phase::RELEASE_WAIT; _phaseMs = now; }
        break;
    case Phase::RELEASE_WAIT:
        // 끊은 뒤 잠깐 기다려 전류가 없을 때 뗀다
        if (wantOn) { _lostWarned = false; _ready = true; _phase = Phase::ON; _phaseMs = now; break; }
        if (inPhase >= RELAY_RELEASE_DELAY_MS) {
            _fetA = _fetB = false;
            _offMs = now;
            _decayCheck = true;
            _phase = Phase::OFF; _phaseMs = now;
        }
        break;
    }
}
