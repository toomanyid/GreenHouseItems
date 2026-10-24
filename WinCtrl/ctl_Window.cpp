#include "ctl_Window.h"
#include "Settings.h"

// ============================================================
//  설정에서 나오는 값
// ============================================================
cs_t Window::sensorPoint() const { return SettingsRules::sensorPoint(_s); }
cs_t Window::maxPos() const      { return SettingsRules::maxPos(_s, sensorActive()); }
cs_t Window::limitPos() const    { return limitEnabled() ? _s.limit : maxPos(); }

uint32_t Window::maxRunMs() const {
    if (_s.maxRunS > 0) return (uint32_t)_s.maxRunS * 1000;
    if (_s.totalUp > 0) return (uint32_t)_s.totalUp * 12;      // 전체 상승 시간(cs) × 10ms × 1.2
    return 0;
}

int32_t Window::overloadMa(int8_t dir) const {
    int32_t base = dir > 0 ? _s.iMaxUp : _s.iMaxDn;
    if (base < 0) return fixedLimitMa();                         // 기준이 비어 있으면 고정 상한
    return base * _s.ovlRate / 10;
}

// 기본값이 "없음"인 항목이 비어 있으면 이동을 받지 않는다 (스펙 8절).
// 센서가 없거나 끈 창은 겹침 하강 시간 대신 원점 시간이 있어야 한다.
bool Window::configured() const {
    if (_s.upTime < 0 || _s.downTime < 0) return false;
    if (_s.iMaxUp < 0 || _s.iMaxDn < 0) return false;
    if (maxRunMs() == 0) return false;
    if (!sensorActive()) return _s.homeTime >= 0;
    // 센서 유무가 아직 정해지지 않았으면 어느 쪽 설정이든 갖춰져 있으면 받는다
    if (_known == Known::UNDECIDED) return _s.overlap >= 0 || _s.homeTime >= 0;
    return _s.overlap >= 0;
}

// ============================================================
//  시작과 설정
// ============================================================
void Window::begin(uint8_t index, Model model, uint32_t now) {
    _index = index;
    _model = model;
    _state = WinState::IDLE;
    _posState = PosState::UNKNOWN;
    _bootGuardMs = now + REVERSE_WAIT_MS;     // 전원을 켠 뒤 첫 구동 전에도 1초를 기다린다
    _lastTickMs = now;
}

void Window::settingsChanged(const WindowSettings& s, bool timesChanged, bool calSaved) {
    _s = s;
    if (calSaved) {
        // 측정 직후에는 하한에 있으므로 닫힘으로 둔다
        _posState = PosState::CLOSED;
        _pos = 0;
        _cal.phase = CalPhase::NONE;
    } else if (timesChanged) {
        _posState = PosState::UNKNOWN;
    }
    if (_state == WinState::IDLE || _state == WinState::LIMIT) settleIdle();
}

// OFF인 채로 처음 하한에 닿음: 센서 "없음"으로 정한다 (스펙 10절)
void Window::decideAbsentAtBottom(uint32_t now) {
    (void)now;
    _known = Known::ABSENT;
    // 에러 기록이 있고 설정이 "자동"인데 센서가 없으면 다시 에러를 내고 없는 것처럼 동작한다
    if (_sensorHistory) {
        _sensorErrMode = true;
        pushEvent(Cause::HOME_SENSOR);
    }
}

// ============================================================
//  잡과 얼럿
// ============================================================
bool Window::submit(const Job& job) {
    if (_state == WinState::FAULT || _state == WinState::ESTOP) return false;
    if (_state == WinState::CALIBRATING) return false;

    if (job.type == JobType::CALIBRATE) {
        if (_state != WinState::IDLE && _state != WinState::LIMIT) return false;
        if (_s.limit != CS_EMPTY) return false;                  // 리밋을 먼저 풀어야 함
        if (!sensorActive() && _s.homeTime < 0) return false;    // 센서 없는 측정은 원점 시간이 필요
    } else if (job.type == JobType::MOVE || job.type == JobType::CLOSE || job.type == JobType::HOME) {
        if (!configured()) return false;
    } else {
        return false;
    }
    _pending = job;
    _pendingValid = true;
    return true;
}

void Window::stop(uint32_t now) {
    _pendingValid = false;
    switch (_state) {
    case WinState::FAULT:
        if (_fPhase != FaultPhase::HOLD) { cut(now); _fPhase = FaultPhase::HOLD; }
        return;
    case WinState::ESTOP:
        return;
    case WinState::CALIBRATING:
        calFail(Cause::NONE, now);
        return;
    case WinState::IDLE:
    case WinState::LIMIT:
        _goal = Goal::NONE;
        return;
    default:
        if (_state == WinState::REVERSE_WAIT && _resume == WinState::CALIBRATING) {
            calFail(Cause::NONE, now);
            return;
        }
        cut(now);
        _goal = Goal::NONE;
        settleIdle();
        return;
    }
}

void Window::clearFault(uint32_t now) {
    if (_state != WinState::FAULT) return;
    cut(now);
    _fault = Cause::NONE;
    _fPhase = FaultPhase::HOLD;
    _clearMs = now;
    _everCleared = true;
    settleIdle();
}

void Window::restoreFault(Cause c) {
    _state = WinState::FAULT;
    _fault = c;
    _fPhase = FaultPhase::HOLD;
}

void Window::recheckSensor() {
    // 센서 판단을 처음으로 돌린다. 여전히 하한에서 OFF거나 위치 확인에서 어긋나면 다시 에러가 난다
    _sensorErrMode = false;
    _known = Known::UNDECIDED;
    _sensorHistory = true;
}

void Window::setEstop(bool on, uint32_t now) {
    if (on) {
        if (_state == WinState::ESTOP) return;
        if (_state == WinState::FAULT) {
            if (_fPhase != FaultPhase::HOLD) { cut(now); _fPhase = FaultPhase::HOLD; }
            return;
        }
        bool cal = _state == WinState::CALIBRATING
                || (_state == WinState::REVERSE_WAIT && _resume == WinState::CALIBRATING);
        cut(now);
        _goal = Goal::NONE;
        _pendingValid = false;
        if (cal) {
            _cal.phase = CalPhase::FAILED;
            _cal.failCause = Cause::NONE;
            _posState = PosState::UNKNOWN;
        }
        _state = WinState::ESTOP;
    } else if (_state == WinState::ESTOP) {
        settleIdle();     // 스스로 다시 움직이지 않는다. 위치는 유지한다.
    }
}

void Window::forceFault(Cause c, uint32_t now) {
    if (_state == WinState::FAULT && _fault == c) return;
    if (_state == WinState::CALIBRATING) {
        _cal.phase = CalPhase::FAILED;
        _cal.failCause = c;
        _posState = PosState::UNKNOWN;
    }
    cut(now);
    _state = WinState::FAULT;
    _fault = c;
    _fPhase = FaultPhase::HOLD;
    _goal = Goal::NONE;
    _pendingValid = false;
}

// ============================================================
//  한 주기
// ============================================================
WindowOutputs Window::tick(const WindowInputs& in) {
    uint32_t now = in.now;
    uint32_t dt = _ticked ? now - _lastTickMs : 0;
    _ticked = true;
    _lastTickMs = now;
    _currentMa = in.currentMa;
    _passedThisTick = false;

    // 1. 지난 주기의 출력으로 위치를 쌓는다
    accumulate(dt);

    // 2. MC가 붙었으면 소프트 스타트를 시작한다
    if (_dir != 0 && _waitRelay && in.relayReady) {
        _waitRelay = false;
        _driveStartMs = now;
        _mon.start();
    }

    // 3. 원점 센서
    handleSensor(in);

    // 4. 구동 중 판정
    if (_dir != 0 && !_waitRelay) checkRunning(in);

    // 5. 다음 목표
    applyPending(now);

    // 6. 현재 상태의 일
    switch (_state) {
    case WinState::IDLE:         break;
    case WinState::LIMIT:        break;
    case WinState::ESTOP:        break;
    case WinState::HOMING:       tickHoming(in); break;
    case WinState::MOVING:       tickMoving(in); break;
    case WinState::CLOSING:      tickClosing(in); break;
    case WinState::REVERSE_WAIT: tickReverseWait(in); break;
    case WinState::CALIBRATING:  tickCalibrating(in); break;
    case WinState::FAULT:        tickFault(in); break;
    }

    // 7. 출력 (소프트 스타트: 0에서 100%까지 일정하게)
    if (_dir != 0 && !_waitRelay) {
        uint32_t r = now - _driveStartMs;
        _duty = r >= SOFT_START_MS ? DUTY_FULL : (uint16_t)(r * DUTY_FULL / SOFT_START_MS);
    } else {
        _duty = 0;
    }

    WindowOutputs out;
    out.dir = _waitRelay ? 0 : _dir;
    out.duty = _duty;
    out.wantRelay = _dir != 0
        || _state == WinState::REVERSE_WAIT
        || (_state == WinState::CALIBRATING && (_cal.phase == CalPhase::WAIT_1 || _cal.phase == CalPhase::WAIT_2))
        || (_state == WinState::FAULT && _fPhase != FaultPhase::HOLD);

    _lastSensor = in.sensor;
    return out;
}

// ============================================================
//  구동과 위치 계산
// ============================================================
void Window::startDrive(int8_t dir, uint32_t now) {
    (void)now;
    _dir = dir;
    _waitRelay = true;
    _runRaw = 0;
    _acc = 0;
    _upPassActive = false;
    _moveMax = 0;
    _mon.reset();
    if (_state != WinState::CALIBRATING && _startsWithoutPass < 0xFFFF) _startsWithoutPass++;
}

void Window::cut(uint32_t now) {
    if (_dir != 0 && !_waitRelay) {
        _lastCutDir = _dir;
        _lastCutMs = now;
    }
    _dir = 0;
    _duty = 0;
    _waitRelay = false;
    _acc = 0;                 // 정지하면 acc를 비운다
    _upPassActive = false;
    _mon.reset();
}

void Window::accumulate(uint32_t dt) {
    if (_dir == 0 || _waitRelay || _duty == 0 || dt == 0) return;
    int64_t add = (int64_t)_duty * dt / 10;           // 전속 10ms = 1000 = 1cs
    _runRaw += add;
    if (_dir < 0 && _s.upTime > 0 && _s.downTime > 0) {
        add = add * _s.upTime / _s.downTime;           // 내려갈 때는 상승 기준으로 환산
    }
    _acc += (int32_t)add;
    while (_acc >= 1000) { _pos += _dir; _acc -= 1000; }
}

// ============================================================
//  목표로 움직이기 시작
// ============================================================
bool Window::needWait(int8_t dir, uint32_t now) const {
    if ((int32_t)(now - _bootGuardMs) < 0) return true;
    return _lastCutDir != 0 && _lastCutDir != dir && now - _lastCutMs < REVERSE_WAIT_MS;
}

void Window::goReverseWait(WinState resume, int8_t dir, uint32_t now) {
    if (_dir != 0) cut(now);
    _state = WinState::REVERSE_WAIT;
    _resume = resume;
    _resumeDir = dir;
}

void Window::startState(WinState st, int8_t dir, uint32_t now) {
    if (needWait(dir, now)) {
        goReverseWait(st, dir, now);
        return;
    }
    _state = st;
    startDrive(dir, now);
}

bool Window::needHoming() const {
    if (_posState == PosState::UNKNOWN) return true;
    if (_posState == PosState::CLOSED) return false;
    // 센서를 지나지 않은 채 기동한 횟수가 설정값을 넘으면 원점을 다시 잡는다.
    // 온도제어에서 온 이동은 0.0초나 CLOSE로 내려가는 이동 때까지 미룬다.
    if (!sensorActive() || _s.rehome == 0) return false;
    if (_startsWithoutPass <= _s.rehome) return false;
    return _goalSource != JobSource::TEMP_CONTROL || _target == 0;
}

void Window::startHoming(uint32_t now) {
    _posState = PosState::UNKNOWN;
    _phaseMs = now;
    if (sensorActive()) {
        _hPhase = (_lastSensor == SensorState::PRESENT) ? HomePhase::EXTRA_DOWN : HomePhase::DOWN_TO_SENSOR;
    } else {
        _hPhase = HomePhase::NOSENSOR_DOWN;
    }
    startState(WinState::HOMING, -1, now);
}

void Window::beginGoal(uint32_t now) {
    _closeSensorSeen = false;
    switch (_goal) {
    case Goal::NONE:
        settleIdle();
        return;
    case Goal::CLOSE:
        if (_posState == PosState::CLOSED) { _goal = Goal::NONE; settleIdle(); return; }
        startState(WinState::CLOSING, -1, now);
        return;
    case Goal::TARGET:
        // 하한에서 센서 "없음"으로 정해졌는데 원점 시간이 없으면 CLOSE 외에는 가지 않는다
        if (!configured()) { _goal = Goal::NONE; settleIdle(); return; }
        if (needHoming()) { startHoming(now); return; }
        if (_posState == PosState::CLOSED) {
            if (sensorActive()) {
                // 하한에서 올라가며 센서 지점을 잡고 그대로 목표로 간다
                _hPhase = HomePhase::UP;
                _phaseMs = now;
                startState(WinState::HOMING, +1, now);
            } else {
                // 센서 없음: 원점 시간만큼 올라간 자리가 0.0초
                _pos = -_s.homeTime;
                _acc = 0;
                _posState = PosState::VALID;
                startState(WinState::MOVING, +1, now);
            }
            return;
        }
        if (_target == _pos) { _goal = Goal::NONE; settleIdle(); return; }
        startState(WinState::MOVING, _target > _pos ? 1 : -1, now);
        return;
    }
}

void Window::applyPending(uint32_t now) {
    if (!_pendingValid) return;
    Job j = _pending;
    _pendingValid = false;

    if (_state == WinState::FAULT || _state == WinState::ESTOP || _state == WinState::CALIBRATING) return;

    if (j.type == JobType::CALIBRATE) {
        if (_state != WinState::IDLE && _state != WinState::LIMIT) return;
        _cal = CalStatus();
        _cal.phase = CalPhase::DOWN_FIRST;
        _calSensor = sensorActive();
        _cal.sensorMode = _calSensor;
        _calMark = false;
        _posState = PosState::UNKNOWN;
        _goal = Goal::NONE;
        startState(WinState::CALIBRATING, -1, now);
        return;
    }

    Goal g = (j.type == JobType::CLOSE) ? Goal::CLOSE : Goal::TARGET;
    cs_t t = j.target;
    if (j.type == JobType::HOME) {
        _posState = PosState::UNKNOWN;
        t = 0;
    }
    if (g == Goal::TARGET) {
        // 목표는 받을 때 상한 리밋으로 자른다
        if (t < 0) t = 0;
        if (t > limitPos()) t = limitPos();
    }
    _goalSource = j.source;

    switch (_state) {
    case WinState::IDLE:
    case WinState::LIMIT:
        _goal = g;
        _target = t;
        beginGoal(now);
        break;

    case WinState::REVERSE_WAIT:
        // 정지가 끝나면 목표를 다시 보고 방향을 정한다
        _goal = g;
        _target = t;
        break;

    case WinState::MOVING:
        if (g == Goal::CLOSE) {
            _goal = Goal::CLOSE;
            if (_dir < 0) { _state = WinState::CLOSING; _closeSensorSeen = false; }
            else goReverseWait(WinState::CLOSING, -1, now);
        } else {
            _goal = Goal::TARGET;
            _target = t;
            int8_t nd = t > _pos ? 1 : (t < _pos ? -1 : 0);
            if (nd == 0) { cut(now); _goal = Goal::NONE; settleIdle(); }
            else if (nd != _dir) goReverseWait(WinState::MOVING, nd, now);
            // 방향이 같으면 목표만 바꾼다
        }
        break;

    case WinState::HOMING:
        if (g == Goal::CLOSE) {
            _goal = Goal::CLOSE;
            if (_dir < 0) { _state = WinState::CLOSING; _closeSensorSeen = false; }
            else goReverseWait(WinState::CLOSING, -1, now);
        } else {
            _goal = Goal::TARGET;
            _target = t;
        }
        break;

    case WinState::CLOSING:
        if (g == Goal::CLOSE) break;
        _goal = Goal::TARGET;
        _target = t;
        if (_posState == PosState::VALID) {
            if (t < _pos) _state = WinState::MOVING;
            else if (t > _pos) goReverseWait(WinState::MOVING, +1, now);
            else { cut(now); _goal = Goal::NONE; settleIdle(); }
        }
        // 위치를 모르면 닫힌 뒤 그 목표로 간다
        break;

    default:
        break;
    }
}

void Window::settleIdle() {
    bool atLimit = _posState == PosState::VALID && limitEnabled() && _pos >= limitPos();
    _state = atLimit ? WinState::LIMIT : WinState::IDLE;
}

// ============================================================
//  판정
// ============================================================
void Window::handleSensor(const WindowInputs& in) {
    SensorState s = in.sensor;
    // 입력이 ON이 되면 센서 "있음"으로 정한다
    if (_known == Known::UNDECIDED && _s.homeSensAuto && !_sensorErrMode && s == SensorState::PRESENT)
        _known = Known::PRESENT;
    if (!sensorActive()) return;
    if (s == SensorState::UNKNOWN) return;           // 전원 켠 직후 첫 판정 전
    // 올라가며 센서가 "없음"으로 바뀌는 순간: 센서 지점으로 고쳐 쓴다
    if (_lastSensor == SensorState::PRESENT && s == SensorState::ABSENT && _dir > 0 && !_waitRelay) {
        _passedThisTick = true;
        if (_state != WinState::CALIBRATING) {
            _pos = sensorPoint();
            _acc = 0;
            _posState = PosState::VALID;
        }
        _startsWithoutPass = 0;
        _upPassActive = true;
        _upPassStart = runCs();
    }
    // 위치 확인: 센서 지점보다 5초 넘게 위에 있는데 ON
    if (_known == Known::PRESENT && _state != WinState::CALIBRATING && _posState == PosState::VALID
        && s == SensorState::PRESENT && _pos > sensorPoint() + POS_CHECK_MARGIN) {
        sensorFailure(in.now);
    }
}

void Window::sensorFailure(uint32_t now) {
    if (!sensorActive()) return;
    _sensorErrMode = true;            // 설정은 바꾸지 않고 센서 없는 것처럼 동작한다
    pushEvent(Cause::HOME_SENSOR);

    if (_s.homeTime < 0) {            // 원점 시간이 없으면 넘기지 못한다
        raiseFault(Cause::SENSOR, now);
        return;
    }
    if (_state == WinState::CALIBRATING) {
        calFail(Cause::SENSOR, now);
        return;
    }
    // 원점을 잡던 중이면 하한 기준으로 다시 잡는다. 그 밖에는 하던 이동을 이어간다.
    if (_state == WinState::HOMING) {
        _hPhase = HomePhase::NOSENSOR_DOWN;
        if (_dir > 0) goReverseWait(WinState::HOMING, -1, now);
    } else if (_state == WinState::REVERSE_WAIT && _resume == WinState::HOMING) {
        _hPhase = HomePhase::NOSENSOR_DOWN;
        _resumeDir = -1;
    }
}

void Window::checkRunning(const WindowInputs& in) {
    uint32_t now = in.now;
    _mon.sample(in.currentMa);
    if (_mon.moveMax() > _moveMax) _moveMax = _mon.moveMax();

    bool cal = _state == WinState::CALIBRATING;
    bool backoff = _state == WinState::FAULT;

    if (in.driverFault) {
        if (backoff) { cut(now); _fPhase = FaultPhase::HOLD; }
        else raiseFault(Cause::DRIVER, now);
        return;
    }
    if (!backoff && _mon.pastStartup() && _mon.windowFull()
        && _mon.average() > (cal ? fixedLimitMa() : overloadMa(_dir))) {
        overload(now);
        return;
    }
    if (_mon.pastStartup() && !_mon.everFlowed()) {
        onNoCurrent(now, in.sensor);
        return;
    }
    if (_mon.stopped()) {
        onCurrentStopped(now, in.sensor);
        return;
    }
    uint32_t limit = cal ? CAL_TIMEOUT_MS : maxRunMs();
    if (limit == 0) limit = MAX_RUN_FALLBACK_MS;
    if (!backoff && now - _driveStartMs > limit) {
        if (cal) calFail(Cause::TIMEOUT, now);
        else raiseFault(Cause::TIMEOUT, now);
    }
}

void Window::onNoCurrent(uint32_t now, SensorState sensor) {
    if (_state == WinState::FAULT) { cut(now); _fPhase = FaultPhase::HOLD; return; }

    if (_dir < 0) {
        // 측정의 첫 하강: 이미 하한에 있어 전류가 흐르지 않음
        if (_state == WinState::CALIBRATING && _cal.phase == CalPhase::DOWN_FIRST) {
            cut(now);
            calAtBottom(sensor, now);
            return;
        }
        // 닫기나 원점 잡기에서 내려가려는데 처음부터 전류가 없음: 이미 하한으로 본다
        bool downward = _state == WinState::CLOSING || _state == WinState::HOMING;
        if (downward && (!sensorActive() || _known == Known::UNDECIDED || sensor == SensorState::PRESENT)) {
            onCurrentStopped(now, sensor);
            return;
        }
    }
    if (_state == WinState::CALIBRATING) { calFail(Cause::NO_CURRENT, now); return; }
    raiseFault(Cause::NO_CURRENT, now);
}

void Window::onCurrentStopped(uint32_t now, SensorState sensor) {
    int8_t d = _dir;

    if (_state == WinState::FAULT) { cut(now); _fPhase = FaultPhase::HOLD; return; }

    if (_state == WinState::CALIBRATING) {
        // 끊김을 확인하는 동안(전속 기준 한 주기에 1cs) 쌓인 시간은 뺀다
        cs_t run = runCs() - _mon.lowRun();
        int32_t imax = _mon.settledMax();
        int32_t iavg = _mon.settledAvg();
        cut(now);
        switch (_cal.phase) {
        case CalPhase::DOWN_FIRST:
            calAtBottom(sensor, now);
            break;
        case CalPhase::UP:
            if (_calSensor && !_calMark) { calFail(Cause::SENSOR, now); return; }
            _cal.totalUp = run;
            _cal.effUp = _calSensor ? run - _calMarkRun : run - _s.homeTime;
            _cal.iMaxUp = imax;
            _cal.iAvgUp = iavg;
            _cal.phase = CalPhase::WAIT_2;
            break;
        case CalPhase::DOWN:
            if (_calSensor && !_calMark) { calFail(Cause::SENSOR, now); return; }
            _cal.totalDown = run;
            if (_calSensor) {
                _cal.effDown = _calMarkRun;
                _cal.overlap = run - _calMarkRun;
            } else {
                _cal.effDown = run - _s.homeTime;
                _cal.overlap = CS_EMPTY;
            }
            _cal.iMaxDn = imax;
            _cal.iAvgDn = iavg;
            _posState = PosState::CLOSED;
            _pos = 0;
            _cal.outOfRange = !SettingsRules::calInRange(_cal);
            _cal.phase = _cal.outOfRange ? CalPhase::FAILED : CalPhase::DONE;
            _state = WinState::IDLE;
            break;
        default:
            break;
        }
        return;
    }

    cut(now);
    if (d < 0) {
        // 하한에서 전류가 끊김: 닫힘
        _posState = PosState::CLOSED;
        _pos = 0;
        if (sensorActive() && sensor != SensorState::PRESENT) {
            if (_known == Known::UNDECIDED) {
                decideAbsentAtBottom(now);     // OFF인 채로 처음 하한: 센서 없음으로 이어간다
            } else {
                sensorFailure(now);            // 센서가 있는 창이 닫혔는데 OFF
                if (_state == WinState::FAULT) return;
            }
        }
        if (_state == WinState::CLOSING) {
            if (_goal == Goal::CLOSE) { _goal = Goal::NONE; settleIdle(); }
            else beginGoal(now);       // 위치를 몰라 먼저 닫았던 목표
            return;
        }
        if (_state == WinState::HOMING) {
            if (_goal != Goal::TARGET) { settleIdle(); return; }
            if (sensorActive()) {
                _hPhase = HomePhase::UP;
                goReverseWait(WinState::HOMING, +1, now);
            } else if (_s.homeTime < 0) {
                _goal = Goal::NONE;            // 원점 시간이 없어 CLOSE에 머문다
                settleIdle();
            } else {
                _pos = -_s.homeTime;
                _posState = PosState::VALID;
                goReverseWait(WinState::MOVING, +1, now);
            }
            return;
        }
        raiseFault(Cause::LIMIT_HIT, now);
    } else {
        // 상한에서 전류가 끊김: 최대 위치로 고쳐 쓴다
        if (_posState == PosState::VALID) _pos = maxPos();
        raiseFault(Cause::LIMIT_HIT, now);
    }
}

// 측정의 첫 하강에서 하한에 닿음: 센서 판단 뒤 1초 정지
void Window::calAtBottom(SensorState sensor, uint32_t now) {
    if (_calSensor && sensor != SensorState::PRESENT && _known == Known::UNDECIDED) {
        decideAbsentAtBottom(now);
        _calSensor = false;            // 센서 없는 측정으로 이어간다
        _cal.sensorMode = false;
        if (_s.homeTime < 0) { calFail(Cause::SENSOR, now); return; }
    }
    _cal.phase = CalPhase::WAIT_1;
}

void Window::raiseFault(Cause c, uint32_t now) {
    if (_state == WinState::CALIBRATING
        || (_state == WinState::REVERSE_WAIT && _resume == WinState::CALIBRATING)) {
        _cal.phase = CalPhase::FAILED;
        _cal.failCause = c;
        _posState = PosState::UNKNOWN;
    }
    cut(now);
    // 해제 후 바로 다시 고장 나면 위치를 "모름"으로 돌린다
    if (_everCleared && now - _clearMs < REFAULT_WINDOW_MS) _posState = PosState::UNKNOWN;
    _state = WinState::FAULT;
    _fault = c;
    _fPhase = FaultPhase::HOLD;
    _goal = Goal::NONE;
    _pendingValid = false;
    pushEvent(c);
}

void Window::overload(uint32_t now) {
    int8_t d = _dir;
    raiseFault(Cause::OVERLOAD, now);
    // 끊고, 1초 뒤 반대로 1.5초 되돌린 다음 FAULT
    _fPhase = FaultPhase::BACKOFF_WAIT;
    _backoffDir = (int8_t)-d;
}

void Window::calFail(Cause c, uint32_t now) {
    cut(now);
    _cal.phase = CalPhase::FAILED;
    _cal.failCause = c;
    _posState = PosState::UNKNOWN;
    _goal = Goal::NONE;
    _state = WinState::IDLE;
}

// ============================================================
//  상태별 일
// ============================================================
void Window::tickHoming(const WindowInputs& in) {
    uint32_t now = in.now;
    switch (_hPhase) {
    case HomePhase::DOWN_TO_SENSOR:
        if (!_waitRelay && _dir < 0 && in.sensor == SensorState::PRESENT) {
            _hPhase = HomePhase::EXTRA_DOWN;
            _phaseMs = now;
        }
        break;
    case HomePhase::EXTRA_DOWN:
        if (_waitRelay || _dir == 0) { _phaseMs = now; break; }
        if (now - _phaseMs >= HOME_EXTRA_DOWN_MS) {
            _hPhase = HomePhase::UP;
            goReverseWait(WinState::HOMING, +1, now);
        }
        break;
    case HomePhase::UP: {
        if (_passedThisTick) {
            // 목표가 센서 지점보다 위면 멈추지 않고 계속 간다. 아래면 끊고 1초 정지 후 내려간다.
            if (_goal == Goal::TARGET && _target > _pos) {
                _state = WinState::MOVING;
            } else {
                goReverseWait(WinState::MOVING, -1, now);
            }
            break;
        }
        if (_waitRelay || _dir == 0) break;
        uint32_t limitMs;
        if (_s.overlap >= 0) {
            limitMs = (uint32_t)(_s.overlap + _s.backoff) * 10 * 2 + 2 * HOME_EXTRA_DOWN_MS + 5000;
        } else {
            limitMs = maxRunMs() ? maxRunMs() : MAX_RUN_FALLBACK_MS;
        }
        if (now - _driveStartMs > limitMs) raiseFault(Cause::SENSOR, now);   // 제한 시간 안에 전환이 없음
        break;
    }
    case HomePhase::NOSENSOR_DOWN:
        break;     // 하한에서 전류가 끊기면 onCurrentStopped가 이어간다
    }
}

void Window::tickMoving(const WindowInputs& in) {
    uint32_t now = in.now;
    if (_dir > 0 && !_waitRelay) {
        // 상한 리밋: 현재 위치가 리밋에 닿으면 끊는다
        bool hit = limitEnabled() && _pos >= limitPos();
        // 센서를 지난 뒤 연속으로 올린 시간이 리밋 값을 넘으면 끊는다
        if (!hit && _upPassActive && limitEnabled()
            && runCs() - _upPassStart > limitPos() - sensorPoint() + LIMIT_BACKUP_MARGIN) {
            hit = true;
        }
        if (hit) {
            cut(now);
            _goal = Goal::NONE;
            _state = WinState::LIMIT;
            return;
        }
    }
    if (_dir == 0) return;
    bool arrived = (_dir > 0 && _pos >= _target) || (_dir < 0 && _pos <= _target);
    if (arrived) {
        cut(now);
        _goal = Goal::NONE;
        settleIdle();
    }
}

void Window::tickClosing(const WindowInputs& in) {
    if (_waitRelay || _dir == 0) return;
    if (!sensorActive() || _s.overlap < 0) return;
    // 센서가 "있음"이 된 뒤 겹침 하강 시간에 여유를 더한 시간이 지나도 전류가 끊기지 않으면 그 자리에서 끊는다
    if (!_closeSensorSeen && in.sensor == SensorState::PRESENT) {
        _closeSensorSeen = true;
        _closeSensorRun = runCs();
    }
    if (_closeSensorSeen && runCs() - _closeSensorRun > _s.overlap + CLOSE_MARGIN) {
        cut(in.now);
        _posState = PosState::CLOSED;
        _pos = 0;
        if (_goal == Goal::CLOSE) { _goal = Goal::NONE; settleIdle(); }
        else beginGoal(in.now);
    }
}

void Window::tickReverseWait(const WindowInputs& in) {
    uint32_t now = in.now;
    if (now - _lastCutMs < REVERSE_WAIT_MS) return;
    if ((int32_t)(now - _bootGuardMs) < 0) return;

    if (_resume == WinState::CALIBRATING) {
        _state = WinState::CALIBRATING;
        startDrive(_resumeDir, now);
    } else if (_resume == WinState::HOMING && _goal == Goal::TARGET) {
        _state = WinState::HOMING;
        _phaseMs = now;
        startDrive(_resumeDir, now);
    } else {
        beginGoal(now);
    }
}

void Window::tickCalibrating(const WindowInputs& in) {
    uint32_t now = in.now;
    _cal.currentMa = in.currentMa;
    switch (_cal.phase) {
    case CalPhase::DOWN_FIRST:
        _cal.elapsed = runCs();
        break;
    case CalPhase::UP:
        _cal.elapsed = runCs();
        if (_calSensor && !_calMark && _passedThisTick) {
            _calMark = true;
            _calMarkRun = runCs();
        }
        break;
    case CalPhase::DOWN:
        _cal.elapsed = runCs();
        if (_calSensor && !_calMark && !_waitRelay
            && _lastSensor == SensorState::ABSENT && in.sensor == SensorState::PRESENT) {
            _calMark = true;
            _calMarkRun = runCs();
        }
        break;
    case CalPhase::WAIT_1:
        _cal.elapsed = 0;
        if (now - _lastCutMs >= REVERSE_WAIT_MS) {
            _cal.phase = CalPhase::UP;
            _calMark = false;
            startDrive(+1, now);
        }
        break;
    case CalPhase::WAIT_2:
        _cal.elapsed = 0;
        if (now - _lastCutMs >= REVERSE_WAIT_MS) {
            _cal.phase = CalPhase::DOWN;
            _calMark = false;
            startDrive(-1, now);
        }
        break;
    default:
        break;
    }
}

void Window::tickFault(const WindowInputs& in) {
    uint32_t now = in.now;
    switch (_fPhase) {
    case FaultPhase::BACKOFF_WAIT:
        if (now - _lastCutMs >= REVERSE_WAIT_MS) {
            _fPhase = FaultPhase::BACKOFF_RUN;
            startDrive(_backoffDir, now);
        }
        break;
    case FaultPhase::BACKOFF_RUN:
        if (!_waitRelay && _dir != 0 && now - _driveStartMs >= OVERLOAD_BACKOFF_MS) {
            cut(now);
            _fPhase = FaultPhase::HOLD;
        }
        break;
    case FaultPhase::HOLD:
        break;
    }
}

// ============================================================
//  상태 복사본과 이벤트
// ============================================================
void Window::fillStatus(WindowStatus& st) const {
    st.state = _state;
    st.posState = _posState;
    st.pos = _pos;
    st.target = (_goal == Goal::TARGET) ? _target : -1;
    st.dir = _waitRelay ? 0 : _dir;
    st.nextDir = (_state == WinState::REVERSE_WAIT) ? _resumeDir : (_waitRelay ? _dir : 0);
    st.fault = _fault;
    st.configured = configured();
    st.sensorFound = _known == Known::PRESENT;
    st.sensorAbsent = _known == Known::ABSENT;
    st.sensorActive = sensorActive();
    st.sensorError = _sensorErrMode && _s.homeSensAuto;
    st.sensor = _lastSensor;
    st.maxPos = maxPos();
    st.limit = limitPos();
    st.currentMa = _currentMa;
    st.moveMaxMa = _moveMax;
    st.overloadUpMa = overloadMa(+1);
    st.overloadDnMa = overloadMa(-1);
    st.cal = _cal;
}

void Window::pushEvent(Cause c) {
    if (_evCount < sizeof(_events) / sizeof(_events[0])) _events[_evCount++] = c;
}

bool Window::popEvent(Cause& c) {
    if (_evCount == 0) return false;
    c = _events[0];
    for (uint8_t i = 1; i < _evCount; i++) _events[i - 1] = _events[i];
    _evCount--;
    return true;
}
