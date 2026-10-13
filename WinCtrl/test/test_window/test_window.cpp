// ctl_Window를 가짜 개폐기(Plant)에 물려 시험한다.
#include "../minitest.h"
#include "../../ctl_Window.h"
#include "../../Settings.h"

// ------------------------------------------------------------
//  가짜 개폐기
//    u: 하한에서 위로 100% 출력으로 올라간 시간 (cs)
//    하한(0)과 상한(top)에서 내장 리밋이 전류를 끊는다
//    u < sensorAt 이면 리밋 스위치 레버가 눌려 "있음"
// ------------------------------------------------------------
struct Plant {
    double  u = 0;
    double  top = 20000;
    double  sensorAt = 1000;
    double  downSpeed = 1.0;     // 내려갈 때 속도 배율
    int32_t loadMa = 2000;
    bool    connected = true;    // 모터 플러그가 자동 소켓에 꽂혀 있음
    bool    sensorBroken = false;

    void step(int8_t dir, uint16_t duty, uint32_t dtMs) {
        double d = (double)duty * dtMs / 10.0 / 1000.0;   // cs
        if (!connected) return;
        if (dir > 0) { u += d; if (u > top) u = top; }
        if (dir < 0) { u -= d * downSpeed; if (u < 0) u = 0; }
    }
    int32_t current(int8_t dir, uint16_t duty) const {
        if (!connected || dir == 0 || duty == 0) return 0;
        if (dir > 0 && u >= top) return 0;
        if (dir < 0 && u <= 0) return 0;
        return loadMa * (duty < 200 ? 200 : duty) / 1000 + 400;
    }
    SensorState sensor() const {
        if (sensorBroken) return SensorState::UNKNOWN;
        return u < sensorAt ? SensorState::PRESENT : SensorState::ABSENT;
    }
};

struct Sim {
    Window   w;
    Plant    p;
    uint32_t now = 0;
    WindowOutputs out = {0, 0, false};
    uint32_t relayOnSince = 0;
    bool     relayOn = false;
    uint32_t noDriveMs = 0;      // 방향 전환 시험용: 출력이 0인 시간

    void begin(const WindowSettings& s, bool sensorFound = true) {
        w.setSettings(s);
        w.begin(0, Model::W50, now);
        w.setSensorFound(sensorFound);
    }
    void step() {
        // 릴레이: 원하면 붙고 0.1초 뒤 준비
        if (out.wantRelay && !relayOn) { relayOn = true; relayOnSince = now; }
        if (!out.wantRelay) relayOn = false;
        bool ready = relayOn && now - relayOnSince >= RELAY_SETTLE_MS;

        WindowInputs in;
        in.now = now;
        in.currentMa = p.current(out.dir, out.duty);
        in.driverFault = false;
        in.sensor = p.sensor();
        in.contactsValid = !p.sensorBroken;
        in.relayReady = ready;
        out = w.tick(in);
        p.step(out.dir, out.duty, CONTROL_PERIOD_MS);
        now += CONTROL_PERIOD_MS;
    }
    void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += CONTROL_PERIOD_MS) step(); }
    // 상태가 바뀔 때까지 (최대 limitMs)
    bool runUntil(WinState st, uint32_t limitMs) {
        for (uint32_t t = 0; t < limitMs; t += CONTROL_PERIOD_MS) {
            step();
            if (w.state() == st) return true;
        }
        return false;
    }
    bool runUntilIdle(uint32_t limitMs) {
        for (uint32_t t = 0; t < limitMs; t += CONTROL_PERIOD_MS) {
            step();
            if (w.state() == WinState::IDLE || w.state() == WinState::LIMIT
                || w.state() == WinState::FAULT) return true;
        }
        return false;
    }
    void move(cs_t target, JobSource src = JobSource::ENCODER) {
        Job j = {};
        j.type = JobType::MOVE;
        j.source = src;
        j.target = target;
        w.submit(j);
    }
    void close() {
        Job j = {};
        j.type = JobType::CLOSE;
        w.submit(j);
    }
};

// 시험용 설정: 전체 상승 200초, 센서는 하한에서 10초 위, 되내림 1.5초
static WindowSettings baseSettings() {
    WindowSettings s;
    SettingsRules::defaultsWindow(s);
    s.upTime = 19000;
    s.downTime = 19000;
    s.overlap = 1000;
    s.backoff = 150;
    s.totalUp = 20000;
    s.iMaxUp = 2500;
    s.iMaxDn = 2500;
    SettingsRules::recomputeHomeTime(s);
    return s;
}

// 위치 0.0초의 실제 자리 = 센서 지점 − 되내림
static const double ZERO_U = 1000 - 150;

static void test_settings_basics() {
    WindowSettings s = baseSettings();
    CHECK(s.homeTime == 850);
    CHECK(SettingsRules::maxPos(s, true) == 19150);
    CHECK(SettingsRules::maxPos(s, false) == 19150);   // 센서가 죽어도 같은 자리
    CHECK(SettingsRules::validWindow(s, true));
    s.limit = 19200;
    CHECK(!SettingsRules::validWindow(s, true));       // 리밋이 최대 위치를 넘음
    s.limit = 10000;
    s.maxRunS = 100;                                   // 상승 190초보다 짧음
    CHECK(!SettingsRules::validWindow(s, true));
}

static void test_not_configured_rejects() {
    Sim sim;
    WindowSettings s;
    SettingsRules::defaultsWindow(s);
    sim.begin(s);
    sim.move(1000);
    sim.run(2000);
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::UNKNOWN);
    CHECK(!sim.w.configured());
}

static void test_power_on_waits() {
    Sim sim;
    sim.p.u = 5000;
    sim.begin(baseSettings());
    sim.run(5000);
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::UNKNOWN);
    CHECK_NEAR(sim.p.u, 5000, 0);                       // 스스로 움직이지 않음
}

static void test_homing_then_target_above() {
    Sim sim;
    sim.p.u = 5000;                                     // 센서 위, 위치 모름
    sim.begin(baseSettings());
    sim.move(3000);
    CHECK(sim.runUntil(WinState::HOMING, 2000));
    CHECK(sim.runUntilIdle(200000));
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::VALID);
    CHECK(sim.w.pos() == 3000);
    CHECK_NEAR(sim.p.u, ZERO_U + 3000, 3);
}

static void test_homing_then_target_below_sensor_point() {
    Sim sim;
    sim.p.u = 500;                                      // 센서 아래 ("있음")
    sim.begin(baseSettings());
    sim.move(0);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.posState() == PosState::VALID);
    CHECK(sim.w.pos() == 0);
    CHECK_NEAR(sim.p.u, ZERO_U, 3);
}

static void test_move_and_reverse_wait() {
    Sim sim;
    sim.p.u = 2000;
    sim.begin(baseSettings());
    sim.move(5000);
    CHECK(sim.runUntilIdle(200000));
    CHECK(sim.w.pos() == 5000);

    sim.move(9000);
    sim.run(3000);
    CHECK(sim.w.state() == WinState::MOVING);
    // 올라가는 중에 아래 목표: 끊고 1초 정지 후 내려감
    sim.move(4000);
    sim.step();
    CHECK(sim.w.state() == WinState::REVERSE_WAIT);
    cs_t posAtTurn = sim.w.pos();
    sim.run(900);
    CHECK(sim.w.state() == WinState::REVERSE_WAIT);
    CHECK(sim.w.pos() == posAtTurn);
    sim.run(200);
    CHECK(sim.w.state() == WinState::MOVING);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.pos() == 4000);
    CHECK_NEAR(sim.p.u, ZERO_U + 4000, 3);
}

static void test_down_ratio() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.downTime = 15200;          // 내려갈 때 1.25배 빠름
    SettingsRules::recomputeHomeTime(s);
    sim.p.downSpeed = 19000.0 / 15200.0;
    sim.p.u = 3000;
    sim.begin(s);
    cs_t sp = SettingsRules::sensorPoint(s);   // 150 × 19000/15200 = 187
    CHECK(sp == 187);
    sim.move(12000);
    CHECK(sim.runUntilIdle(300000));
    double zero = 1000 - 150 * (19000.0 / 15200.0);
    CHECK_NEAR(sim.p.u, zero + 12000, 4);
    sim.move(2000);
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.pos() == 2000);
    CHECK_NEAR(sim.p.u, zero + 2000, 15);
}

static void test_close_and_reopen() {
    Sim sim;
    sim.p.u = 4000;
    sim.begin(baseSettings());
    sim.move(6000);
    CHECK(sim.runUntilIdle(200000));
    sim.close();
    CHECK(sim.runUntil(WinState::CLOSING, 2000));
    CHECK(sim.runUntilIdle(200000));
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::CLOSED);
    CHECK(sim.p.u <= 0.01);
    // 닫힘에서 열기: 올라가며 센서 지점을 잡고 그대로 감
    sim.move(1000);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.pos() == 1000);
    CHECK_NEAR(sim.p.u, ZERO_U + 1000, 3);
}

static void test_close_when_unknown_and_already_closed() {
    Sim sim;
    sim.p.u = 0;                 // 이미 하한. 내려가려 해도 전류가 흐르지 않음
    sim.begin(baseSettings());
    sim.close();
    CHECK(sim.runUntilIdle(10000));
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::CLOSED);
}

static void test_upper_limit() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.limit = 10000;
    sim.p.u = 3000;
    sim.begin(s);
    sim.move(15000);              // 리밋으로 잘림
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.state() == WinState::LIMIT);
    CHECK(sim.w.pos() == 10000);
    // 바깥으로 가는 목표는 움직이지 않음, 안쪽은 움직임
    sim.move(12000);
    sim.run(2000);
    CHECK(sim.w.state() == WinState::LIMIT);
    sim.move(8000);
    CHECK(sim.runUntil(WinState::MOVING, 3000));
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.state() == WinState::IDLE);
}

static void test_overload_backoff() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(15000);
    CHECK(sim.runUntilIdle(200000));
    sim.move(18000);
    sim.run(5000);
    double before = sim.p.u;
    sim.p.loadMa = 4000;          // 이물이 말려 전류가 오름 (기준 2.5A × 1.3)
    CHECK(sim.runUntil(WinState::FAULT, 2000));
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.fault == Cause::OVERLOAD);
    double atFault = sim.p.u;
    CHECK(atFault > before);
    sim.run(4000);                // 1초 뒤 반대로 1.5초 되돌림
    CHECK(sim.p.u < atFault - 50);
    double after = sim.p.u;
    sim.run(3000);
    CHECK_NEAR(sim.p.u, after, 0);
    CHECK(sim.w.state() == WinState::FAULT);
    Cause c;
    CHECK(sim.w.popEvent(c) && c == Cause::OVERLOAD);

    // 해제 후 바로 다시 고장 나면 위치를 모름으로
    sim.p.loadMa = 2000;
    sim.w.clearFault(sim.now);
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::VALID);
    sim.move(16000);
    sim.run(3000);
    sim.p.loadMa = 4000;
    CHECK(sim.runUntil(WinState::FAULT, 3000));
    CHECK(sim.w.posState() == PosState::UNKNOWN);
}

static void test_no_current() {
    Sim sim;
    sim.p.u = 5000;
    sim.p.connected = false;      // 모터 플러그가 수동 소켓에 있음
    sim.begin(baseSettings());
    sim.move(8000);
    CHECK(sim.runUntil(WinState::FAULT, 5000));
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.fault == Cause::NO_CURRENT);
}

static void test_limit_hit_fault_on_top() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(10000);
    CHECK(sim.runUntilIdle(200000));
    sim.p.top = 12000;            // 상한이 생각보다 낮음
    sim.move(15000);
    CHECK(sim.runUntil(WinState::FAULT, 100000));
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.fault == Cause::LIMIT_HIT);
    CHECK(sim.w.pos() == sim.w.maxPos());
}

static void test_timeout() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.maxRunS = 190;
    sim.p.top = 1e9;              // 내장 리밋이 고장 나 끝없이 올라감
    sim.p.u = 3000;
    sim.begin(s);
    sim.move(1000);
    CHECK(sim.runUntilIdle(100000));
    WindowSettings s2 = s;
    s2.upTime = 90000;            // 시간 초과를 보려고 리밋을 크게
    s2.totalUp = 91000;
    sim.w.setSettings(s2);
    sim.move(80000);
    CHECK(sim.runUntil(WinState::FAULT, 200000));
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.fault == Cause::TIMEOUT);
    CHECK_NEAR(sim.now, 190000 + 30000, 40000);
}

static void test_estop() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(10000);
    CHECK(sim.runUntilIdle(200000));
    sim.move(15000);
    sim.run(3000);
    sim.w.setEstop(true, sim.now);
    sim.step();
    CHECK(sim.w.state() == WinState::ESTOP);
    double u = sim.p.u;
    sim.run(2000);
    CHECK_NEAR(sim.p.u, u, 0);
    sim.move(12000);              // 눌려 있는 동안 어떤 이동도 받지 않음
    sim.run(1000);
    CHECK(sim.w.state() == WinState::ESTOP);
    sim.w.setEstop(false, sim.now);
    sim.run(3000);
    CHECK(sim.w.state() == WinState::IDLE);   // 스스로 다시 움직이지 않음
    CHECK(sim.w.posState() == PosState::VALID);
    CHECK_NEAR(sim.p.u, u, 0);
}

static void test_stop_alert() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(10000);
    CHECK(sim.runUntilIdle(200000));
    sim.move(15000);
    sim.run(3000);
    sim.w.stop(sim.now);
    sim.step();
    CHECK(sim.w.state() == WinState::IDLE);
    cs_t p = sim.w.pos();
    CHECK(p > 10000 && p < 15000);
    CHECK_NEAR(sim.p.u, ZERO_U + p, 3);
}

static void test_calibration_with_sensor() {
    Sim sim;
    WindowSettings s;
    SettingsRules::defaultsWindow(s);       // 비어 있는 창
    sim.p.u = 6000;
    sim.begin(s);
    Job j = {};
    j.type = JobType::CALIBRATE;
    CHECK(sim.w.submit(j));
    CHECK(sim.runUntil(WinState::CALIBRATING, 3000));
    for (int i = 0; i < 100000 && sim.w.state() == WinState::CALIBRATING; i++) sim.step();
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.cal.phase == CalPhase::DONE);
    CHECK_NEAR(st.cal.totalUp, 20000, 3);
    CHECK_NEAR(st.cal.effUp, 19000, 3);
    CHECK_NEAR(st.cal.effDown, 19000, 3);
    CHECK_NEAR(st.cal.overlap, 1000, 3);
    CHECK_NEAR(st.cal.iMaxUp, 2400, 50);
    CHECK_NEAR(st.cal.iAvgUp, 2400, 50);
    CHECK(sim.w.posState() == PosState::CLOSED);
    CHECK(sim.p.u <= 0.01);
}

static void test_calibration_refused_with_limit() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.limit = 10000;
    sim.begin(s);
    Job j = {};
    j.type = JobType::CALIBRATE;
    CHECK(!sim.w.submit(j));
}

static void test_calibration_stopped() {
    Sim sim;
    sim.p.u = 6000;
    sim.begin(baseSettings());
    Job j = {};
    j.type = JobType::CALIBRATE;
    sim.w.submit(j);
    sim.run(5000);
    sim.w.stop(sim.now);
    sim.step();
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.cal.phase == CalPhase::FAILED);
    CHECK(st.cal.failCause == Cause::NONE);
    CHECK(sim.w.state() == WinState::IDLE);
    CHECK(sim.w.posState() == PosState::UNKNOWN);
}

static void test_sensor_dies_while_moving() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(10000);
    CHECK(sim.runUntilIdle(200000));
    sim.move(14000);
    sim.run(3000);
    sim.p.sensorBroken = true;
    sim.step();
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.sensorError);
    CHECK(!st.sensorActive);
    CHECK(sim.w.state() == WinState::MOVING);    // 멈추지 않고 이어감
    Cause c;
    CHECK(sim.w.popEvent(c) && c == Cause::HOME_SENSOR);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.pos() == 14000);
    CHECK_NEAR(sim.p.u, ZERO_U + 14000, 3);

    // 센서 없이 닫고 다시 열기: 원점 시간으로 0.0초를 잡음
    sim.close();
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.posState() == PosState::CLOSED);
    sim.move(2000);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.pos() == 2000);
    CHECK_NEAR(sim.p.u, ZERO_U + 2000, 3);

    // 확인하면 다시 확인. 여전히 이상이면 다시 에러
    sim.w.recheckSensor(sim.now);
    sim.run(3500);
    CHECK(sim.w.popEvent(c) && c == Cause::HOME_SENSOR);
    // 접점이 정상으로 돌아오면 에러가 다시 나지 않음
    sim.p.sensorBroken = false;
    sim.w.recheckSensor(sim.now);
    sim.run(3500);
    CHECK(!sim.w.popEvent(c));
    sim.w.fillStatus(st);
    CHECK(!st.sensorError);
    CHECK(st.sensorActive);
}

static void test_sensor_fails_without_home_time() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.homeTime = CS_EMPTY;
    sim.p.u = 3000;
    sim.begin(s);
    sim.move(10000);
    CHECK(sim.runUntilIdle(200000));
    sim.p.sensorBroken = true;
    sim.step();
    CHECK(sim.w.state() == WinState::FAULT);
    WindowStatus st;
    sim.w.fillStatus(st);
    CHECK(st.fault == Cause::SENSOR);
}

static void test_sensorless_homing() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.homeSensAuto = false;       // 센서 사용 끔
    sim.p.u = 7000;
    sim.begin(s);
    sim.move(3000);
    CHECK(sim.runUntil(WinState::HOMING, 3000));
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.pos() == 3000);
    CHECK_NEAR(sim.p.u, ZERO_U + 3000, 3);
}

static void test_rehome_count_and_temp_defer() {
    Sim sim;
    WindowSettings s = baseSettings();
    s.rehome = 3;
    sim.p.u = 3000;
    sim.begin(s);
    sim.move(10000);                                  // 원점 잡기로 센서를 지남
    CHECK(sim.runUntilIdle(200000));
    // 온도제어 이동 네 번: 센서를 지나지 않음
    cs_t t[] = {11000, 12000, 13000, 14000, 15000};
    for (int i = 0; i < 5; i++) {
        sim.move(t[i], JobSource::TEMP_CONTROL);
        sim.step();
        CHECK(sim.w.state() != WinState::HOMING);     // 온도제어 이동은 미룸
        CHECK(sim.runUntilIdle(100000));
    }
    // 엔코더 이동이면 다시 잡음
    sim.move(9000, JobSource::ENCODER);
    sim.run(20);
    CHECK(sim.w.state() == WinState::HOMING || sim.w.state() == WinState::REVERSE_WAIT);
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.pos() == 9000);
    CHECK_NEAR(sim.p.u, ZERO_U + 9000, 3);
}

static void test_new_target_same_direction() {
    Sim sim;
    sim.p.u = 3000;
    sim.begin(baseSettings());
    sim.move(5000);
    CHECK(sim.runUntilIdle(200000));
    sim.move(9000);
    sim.run(2000);
    sim.move(12000);                                   // 같은 방향: 목표만 바뀜
    sim.step();
    CHECK(sim.w.state() == WinState::MOVING);
    CHECK(sim.runUntilIdle(100000));
    CHECK(sim.w.pos() == 12000);
}

static void test_close_on_unknown_then_target() {
    Sim sim;
    sim.p.u = 8000;
    sim.begin(baseSettings());
    sim.close();
    sim.run(5000);
    CHECK(sim.w.state() == WinState::CLOSING);
    sim.move(4000);                                    // 위치를 모르면 닫은 뒤 그 목표로
    CHECK(sim.runUntilIdle(300000));
    CHECK(sim.w.pos() == 4000);
    CHECK_NEAR(sim.p.u, ZERO_U + 4000, 3);
}

// ------------------------------------------------------------
//  RelayControl
// ------------------------------------------------------------
static void test_relay_selftest_and_engage() {
    RelayControl r;
    r.begin(0);
    uint32_t t = 0;
    for (; t < 2000 && !r.selfTestDone(); t += 10) r.tick(t, 0, false);
    CHECK(r.selfTestDone());
    CHECK(!r.fault());
    // 붙일 때 B 먼저
    r.tick(t, 0, true); t += 10;
    CHECK(r.fetB() && !r.fetA());
    r.tick(t, 0, true); t += 10;
    CHECK(r.fetA() && r.fetB());
    for (int i = 0; i < 12; i++) { r.tick(t, 24000, true); t += 10; }
    CHECK(r.ready());
    // 떼기: 0.1초 기다린 뒤
    r.tick(t, 24000, false); t += 10;
    CHECK(!r.ready() && r.fetA());
    for (int i = 0; i < 11; i++) { r.tick(t, 20000, false); t += 10; }
    CHECK(!r.fetA() && !r.fetB());
    // 뗀 뒤 전압이 남으면 접점 붙음
    for (int i = 0; i < 400; i++) { r.tick(t, 20000, false); t += 10; }
    CHECK(r.fault());
}

static void test_relay_selftest_detects_shorted_fet() {
    RelayControl r;
    r.begin(0);
    uint32_t t = 0;
    for (; t < 2000; t += 10) r.tick(t, r.fetA() ? 24000 : 0, false);   // A만 켰는데 전압: B 붙음
    CHECK(r.fault());
}

static void test_current_monitor() {
    CurrentMonitor m;
    m.start();
    for (int i = 0; i < 150; i++) m.sample(i < 100 ? 5000 : 2000);   // 기동 1초는 판정에서 뺌
    CHECK(m.pastStartup());
    CHECK(m.average() == 2000);
    for (int i = 0; i < 100; i++) m.sample(2000);
    for (int i = 0; i < 10; i++) m.sample(0);
    CHECK(m.stopped());
    CHECK(m.settledMax() == 2000);    // 끊기는 순간과 첫 1초는 빠짐
    CHECK(m.settledAvg() == 2000);
}

int main() {
    RUN(test_settings_basics);
    RUN(test_not_configured_rejects);
    RUN(test_power_on_waits);
    RUN(test_homing_then_target_above);
    RUN(test_homing_then_target_below_sensor_point);
    RUN(test_move_and_reverse_wait);
    RUN(test_down_ratio);
    RUN(test_close_and_reopen);
    RUN(test_close_when_unknown_and_already_closed);
    RUN(test_upper_limit);
    RUN(test_overload_backoff);
    RUN(test_no_current);
    RUN(test_limit_hit_fault_on_top);
    RUN(test_timeout);
    RUN(test_estop);
    RUN(test_stop_alert);
    RUN(test_calibration_with_sensor);
    RUN(test_calibration_refused_with_limit);
    RUN(test_calibration_stopped);
    RUN(test_sensor_dies_while_moving);
    RUN(test_sensor_fails_without_home_time);
    RUN(test_sensorless_homing);
    RUN(test_rehome_count_and_temp_defer);
    RUN(test_new_target_same_direction);
    RUN(test_close_on_unknown_then_target);
    RUN(test_relay_selftest_and_engage);
    RUN(test_relay_selftest_detects_shorted_fet);
    RUN(test_current_monitor);
    return finish();
}
