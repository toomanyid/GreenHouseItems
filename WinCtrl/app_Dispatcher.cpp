#include "app_Dispatcher.h"

void Dispatcher::begin(Model model, const Settings& s, uint32_t now) {
    _model = model;
    _s = s;
    _startMs = now;
    _bootDone = false;
    for (uint8_t i = 0; i < 2; i++) {
        _w[i].setSettings(s.win[i]);
        _w[i].begin(i, model, now);
    }
    _relay.begin(now);
    _temp.setSettings(s.temp);
    _temp.begin(now);
    _estop.set(false);
    _tempSum = 0;
    _tempN = 0;
}

void Dispatcher::pushEvent(uint8_t window, Cause c) {
    ErrorEvent& e = _events[_eventCount % EVENT_RING_SIZE];
    e.window = window;
    e.cause = c;
    _eventCount++;
    if (isFaultCause(c)) _temp.resetStep();
}

// 전원 켤 때: 온도 센서 2초, 릴레이 자가 점검.
// 원점 센서 유무는 창이 스스로 정한다 (ON이 들어오거나 OFF인 채로 처음 하한에 닿을 때)
void Dispatcher::bootStep(const ControlInputs& in) {
    uint32_t elapsed = in.now - _startMs;
    if (elapsed < TEMP_DETECT_MS) {
        _tempSum += in.tempUa;
        _tempN++;
    }
    if (elapsed >= TEMP_DETECT_MS && _relay.selfTestDone()) {
        int32_t avg = _tempN ? (int32_t)(_tempSum / _tempN) : 0;
        _tempFound = _s.temp.sensorAuto && avg >= TEMP_PRESENT_UA;
        _tempValid = _tempFound;
        _bootDone = true;
    }
}

void Dispatcher::alert(const Alert& a, uint32_t now) {
    switch (a.type) {
    case AlertType::STOP_ALL:
        _w[0].stop(now);
        _w[1].stop(now);
        _temp.resetStep();
        break;
    case AlertType::CLEAR_FAULT:
        if (_relay.fault()) break;              // 릴레이 고장은 전원을 다시 켤 때까지 남는다
        for (uint8_t i = 0; i < 2; i++) {
            if (a.window == 0xFF || a.window == i) _w[i].clearFault(now);
        }
        _temp.resetStep();
        break;
    case AlertType::RECHECK_SENSOR:
        if (a.window < 2) _w[a.window].recheckSensor();
        break;
    case AlertType::RESTORE_FAULT:
        if (a.window < 2) _w[a.window].restoreFault(a.cause);
        break;
    case AlertType::SENSOR_HISTORY:
        if (a.window < 2) _w[a.window].setSensorHistory();
        break;
    case AlertType::MENU_ENTER:
        _menu = true;
        break;
    case AlertType::MENU_EXIT:
        _menu = false;
        break;
    }
}

bool Dispatcher::job(const Job& j, uint32_t now) {
    (void)now;
    if (!_bootDone || j.window > 1) return false;
    if (j.type == JobType::SETTINGS_CHANGED) return false;
    if (j.source == JobSource::ENCODER) _temp.resetStep();   // 모터에 직접 목표를 넣으면 처음 크기로
    return _w[j.window].submit(j);
}

void Dispatcher::settingsChanged(const Settings& s, uint8_t timesMask, uint8_t calMask) {
    bool wasAuto = _s.temp.sensorAuto;
    _s = s;
    for (uint8_t i = 0; i < 2; i++) {
        _w[i].settingsChanged(s.win[i], (timesMask >> i) & 1, (calMask >> i) & 1);
    }
    _temp.setSettings(s.temp);
    if (!s.temp.sensorAuto) _tempFound = false;
    else if (!wasAuto) _tempFound = _tempValid;   // 끔에서 자동으로: 지금 값이 유효하면 씀
}

ControlOutputs Dispatcher::step(const ControlInputs& in) {
    uint32_t now = in.now;
    ControlOutputs out = {};

    if (!_bootDone) bootStep(in);

    // 비상정지: 버튼 뒤 전압이 없음
    bool pressed = _estop.update(in.estopMv < ESTOP_OK_MV, now, ESTOP_DEBOUNCE_MS);
    for (uint8_t i = 0; i < 2; i++) _w[i].setEstop(pressed, now);
    if (pressed) _temp.resetStep();

    // 릴레이 고장이면 운전을 막는다
    if (_relay.fault()) {
        for (uint8_t i = 0; i < 2; i++) _w[i].forceFault(Cause::RELAY, now);
        if (!_relayFaultLogged) { pushEvent(0, Cause::RELAY); _relayFaultLogged = true; }
    }

    // 창 2개의 상태 흐름
    bool ready = _relay.ready();
    WindowOutputs wo[2];
    for (uint8_t i = 0; i < 2; i++) {
        WindowInputs wi;
        wi.now = now;
        wi.currentMa = in.currentMa[i];
        wi.driverFault = in.driverFault[i];
        wi.sensor = in.sensor[i];
        wi.relayReady = ready;
        wo[i] = _w[i].tick(wi);
    }

    // 주 릴레이: 두 창이 모두 멈췄으면 뗀다
    bool want = (wo[0].wantRelay || wo[1].wantRelay) && _relay.selfTestDone();
    _relay.tick(now, in.relayMv, want);
    out.fetA = _relay.fetA();
    out.fetB = _relay.fetB();
    for (uint8_t i = 0; i < 2; i++) {
        bool run = ready && _relay.ready() && wo[i].dir != 0;
        out.dir[i] = run ? wo[i].dir : 0;
        out.duty[i] = run ? wo[i].duty : 0;
    }

    // 창에서 생긴 에러를 에러 저장으로
    for (uint8_t i = 0; i < 2; i++) {
        Cause c;
        while (_w[i].popEvent(c)) pushEvent((uint8_t)(i + 1), c);
    }

    // 온도 센서
    if (_bootDone && _tempFound) {
        bool lost = in.tempUa < TEMP_PRESENT_UA;
        bool over = in.tempUa > TEMP_OVER_UA;
        if (lost || over) {
            if (!_tempBadPending) { _tempBadPending = true; _tempBadSince = now; }
            if (_tempValid && now - _tempBadSince >= TEMP_LOSS_MS) {
                _tempValid = false;
                pushEvent(0, over ? Cause::TEMP_BAD : Cause::TEMP_LOST);
            }
        } else {
            _tempBadPending = false;
            _tempValid = true;
            _tempValue = TempControl::toTemp(in.tempUa, _s.temp);
        }

        TempControl::WinView v[2];
        for (uint8_t i = 0; i < 2; i++) {
            WinState st = _w[i].state();
            v[i].eligible = _w[i].configured() && st != WinState::FAULT && st != WinState::ESTOP
                         && st != WinState::CALIBRATING;
            v[i].posState = _w[i].posState();
            v[i].pos = _w[i].pos();
            v[i].limit = _w[i].limitPos();
        }
        Job jobs[2];
        uint8_t n = _temp.tick(now, _tempValid, _tempValue, _menu, v, jobs);
        for (uint8_t k = 0; k < n; k++) _w[jobs[k].window].submit(jobs[k]);
    }
    return out;
}

void Dispatcher::fillStatus(Status& st) const {
    for (uint8_t i = 0; i < 2; i++) _w[i].fillStatus(st.win[i]);
    st.temp.found = _tempFound;
    st.temp.valid = _tempFound && _tempValid;
    st.temp.temp = _tempValue;
    st.temp.predicted = _temp.predicted();
    st.temp.slope = _temp.slope();
    st.temp.paused = _menu;
    st.model = _model;
    st.bootDone = _bootDone;
    st.estop = _estop.value();
    st.relayFault = _relay.fault();
    st.eventCount = _eventCount;
    for (uint8_t i = 0; i < EVENT_RING_SIZE; i++) st.events[i] = _events[i];
}
