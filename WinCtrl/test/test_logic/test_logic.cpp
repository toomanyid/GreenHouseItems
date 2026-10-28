// 화면 글자, 에러 저장, 센서 판정, 온도제어, Dispatcher를 PC에서 시험한다.
#include <cstring>
#include "../minitest.h"
#include "../../io_Format.h"
#include "../../ErrorLog.h"
#include "../../ctl_Safety.h"
#include "../../app_TempControl.h"
#include "../../app_Dispatcher.h"
#include "../../dev_Sim.h"
#include "../../Settings.h"

static bool eq(const char* a, const char* b) {
    bool ok = std::strcmp(a, b) == 0;
    if (!ok) std::printf("    got [%s] want [%s]\n", a, b);
    return ok;
}

static WindowStatus idleWin(PosState ps, cs_t pos, cs_t maxPos) {
    WindowStatus w = {};
    w.state = WinState::IDLE;
    w.posState = ps;
    w.pos = pos;
    w.maxPos = maxPos;
    w.limit = maxPos;
    w.target = CS_EMPTY;
    return w;
}

static Fmt::LineCtx ctx(uint8_t num, char sel) {
    Fmt::LineCtx c = {};
    c.num = num; c.sel = sel; c.blinkOn = true;
    return c;
}

// ------------------------------------------------------------
static void test_format_values() {
    char b[24];
    Fmt::secs(b, 18234, 5); CHECK(eq(b, "182.5"));
    Fmt::secs(b, 18224, 5); CHECK(eq(b, "182.0"));
    Fmt::secs(b, 4500, 5);  CHECK(eq(b, " 45.0"));
    Fmt::secs(b, CS_EMPTY, 5); CHECK(eq(b, "---.-"));
    Fmt::secs2(b, 16352); CHECK(eq(b, "163.52"));
    Fmt::secsInt(b, 18250, 4); CHECK(eq(b, " 183"));     // 0.5는 올림
    Fmt::secsInt(b, 18249, 4); CHECK(eq(b, " 182"));
    Fmt::pct(b, 0, 18000); CHECK(eq(b, "  1"));
    Fmt::pct(b, 18000, 18000); CHECK(eq(b, "100"));
    Fmt::amps(b, 2140); CHECK(eq(b, "2.1A"));
    Fmt::temp4(b, 255); CHECK(eq(b, "25.5"));
    Fmt::temp4(b, 50);  CHECK(eq(b, " 5.0"));
    Fmt::temp4(b, -150); CHECK(eq(b, "-9.9"));             // 표시만 -9.9
    Fmt::temp4(b, 1234); CHECK(eq(b, "99.9"));
    Fmt::buildDate(b, "Oct  9 2026", "19:47:12"); CHECK(eq(b, "2026-10-09 19:47"));
    Fmt::splashTop(b, "WIN CTRL", 0, 9); CHECK(eq(b, "WIN CTRL   00.09"));
}

static void test_format_lines() {
    char l[24];
    WindowStatus w = idleWin(PosState::VALID, 18250, 18250);
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 182.5s 100%  "));
    w = idleWin(PosState::VALID, 4500, 18000);
    Fmt::winLine(l, w, ctx(2, ' ')); CHECK(eq(l, " 2  45.0s  25%  "));
    w = idleWin(PosState::CLOSED, 0, 18000);
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1  CLOSE   0%  "));
    w = idleWin(PosState::UNKNOWN, 0, 18000);
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 ---.-s ---% ?"));

    w = idleWin(PosState::VALID, 4500, 18000);
    w.state = WinState::MOVING; w.dir = 1; w.target = 12000;
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1  45.0>120.0 ^"));
    Fmt::LineCtx c = ctx(1, '>'); c.blinkOn = false;
    Fmt::winLine(l, w, c); CHECK(eq(l, ">1  45.0>120.0  "));

    w.state = WinState::REVERSE_WAIT; w.dir = 0; w.nextDir = -1;
    c.blinkOn = false;
    Fmt::winLine(l, w, c); CHECK(l[15] == 'v');             // 전환 정지 중에는 켜 둔다

    w = idleWin(PosState::VALID, 12000, 18000);
    w.state = WinState::LIMIT;
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 120.0s  67% L"));

    w.state = WinState::FAULT; w.fault = Cause::OVERLOAD;
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 ERR OVERLOAD!"));

    w = idleWin(PosState::UNKNOWN, 0, 18000);
    w.state = WinState::HOMING; w.dir = -1;
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 HOME        v"));

    w.state = WinState::CALIBRATING; w.dir = 1; w.cal.elapsed = 6350;
    Fmt::winLine(l, w, ctx(1, '>')); CHECK(eq(l, ">1 CAL  63.5s  ^"));

    w = idleWin(PosState::VALID, 4500, 18000);
    c = ctx(1, '*'); c.editing = true; c.editTarget = 6000;
    Fmt::winLine(l, w, c); CHECK(eq(l, "*1  60.0s  34%  "));
    c.editTarget = -1;
    Fmt::winLine(l, w, c); CHECK(eq(l, "*1  CLOSE   0%  "));

    // 원점 센서 에러: 1초마다 번갈아
    c = ctx(1, '>'); c.sensorErr = true; c.altPhase = true;
    Fmt::winLine(l, w, c); CHECK(eq(l, ">1 ERR SENSOR   "));
}

static void test_format_temp_screen() {
    char r[12], l[24];
    WindowStatus w = idleWin(PosState::VALID, 18250, 18250);
    Fmt::winShort(r, w, ctx(1, '>')); CHECK(eq(r, ">1 183s "));
    Fmt::tempLine(l, ' ', true, 255, 'C', r); CHECK(eq(l, " 25.5C  >1 183s "));
    w = idleWin(PosState::VALID, 4500, 18000);
    Fmt::winShort(r, w, ctx(2, ' '));
    Fmt::tempLine(l, '>', true, 280, 'S', r); CHECK(eq(l, ">28.0S   2  45s "));
    Fmt::tempLine(l, ' ', false, 0, 'C', r); CHECK(eq(l, " --.-C   2  45s "));

    w = idleWin(PosState::VALID, 18250, 18250);
    w.state = WinState::FAULT; w.fault = Cause::OVERLOAD;
    Fmt::LineCtx c = ctx(1, '>');
    c.altPhase = false; Fmt::winShort(r, w, c); CHECK(eq(r, ">1 183s!"));
    c.altPhase = true;  Fmt::winShort(r, w, c); CHECK(eq(r, ">1 E-OVL"));

    w = idleWin(PosState::CLOSED, 0, 18000);
    c.altPhase = true; Fmt::winShort(r, w, c); CHECK(eq(r, ">1 CLOSE"));
    w = idleWin(PosState::UNKNOWN, 0, 18000);
    c.altPhase = false; Fmt::winShort(r, w, c); CHECK(eq(r, ">1 ---s?"));
}

// ------------------------------------------------------------
static void test_error_log() {
    ErrorLog log;
    CHECK(log.add(1, Cause::OVERLOAD, 3, 100));
    CHECK(!log.add(1, Cause::OVERLOAD, 3, 120));            // 같은 창 같은 원인 미확인: 추가 안 함
    CHECK(log.add(2, Cause::OVERLOAD, 3, 130));
    CHECK(log.count() == 2);
    ErrorEntry e;
    CHECK(log.ackOldest(e) && e.window == 1);
    CHECK(log.add(1, Cause::OVERLOAD, 3, 140));             // 확인 뒤에는 다시 추가
    CHECK(log.hasAny(1, Cause::OVERLOAD));
    CHECK(log.newest(0).uptimeS == 140);

    // 가득 차면 확인한 것 중 가장 오래된 것부터
    ErrorLog full;
    for (uint8_t i = 0; i < ERROR_LOG_SIZE; i++) full.add((uint8_t)(i % 3), (Cause)(1 + i / 3), 1, i);
    CHECK(full.count() == ERROR_LOG_SIZE);
    full.ackOldest(e); full.ackOldest(e);
    full.add(0, Cause::TEMP_BAD, 1, 99);
    CHECK(full.count() == ERROR_LOG_SIZE);
    CHECK(full.newest(0).cause == Cause::TEMP_BAD);
    int acked = 0;
    for (uint8_t i = 0; i < full.count(); i++) if (full.at(i).acked) acked++;
    CHECK(acked == 1);
}

// ------------------------------------------------------------
static void test_sensor_filter() {
    SensorFilter f;
    f.begin(0);
    uint32_t t = 0;
    for (; t < 90; t += 10) f.update(true, t);
    CHECK(f.value() == SensorState::UNKNOWN);               // 0.1초가 안 됨
    for (; t <= 110; t += 10) f.update(true, t);
    CHECK(f.value() == SensorState::PRESENT);
    f.update(false, t); t += 10;                             // 한 번 튐: 무시
    f.update(true, t);  t += 10;
    CHECK(f.value() == SensorState::PRESENT);
    for (uint32_t e = t + 110; t <= e; t += 10) f.update(false, t);
    CHECK(f.value() == SensorState::ABSENT);
}

// ------------------------------------------------------------
static void test_temp_control() {
    TempSettings ts;
    SettingsRules::defaultsTemp(ts);
    CHECK(TempControl::toTemp(4000, ts) == 0);
    CHECK(TempControl::toTemp(20000, ts) == 500);
    CHECK(TempControl::toTemp(12000, ts) == 250);
    ts.aheadMin = 0;                                        // 예측 = 현재

    TempControl tc;
    tc.setSettings(ts);
    tc.begin(0);
    TempControl::WinView v[2];
    for (int i = 0; i < 2; i++) { v[i].eligible = true; v[i].posState = PosState::VALID; v[i].pos = 1000; v[i].limit = 18000; }
    Job jobs[2];
    uint32_t t = 0;
    uint8_t n = 0;
    for (; t < 60000; t += 1000) n = tc.tick(t, true, 300, false, v, jobs);
    n = tc.tick(t, true, 300, false, v, jobs);
    CHECK(n == 2);
    CHECK(jobs[0].type == JobType::MOVE && jobs[0].target == 1500 && jobs[0].source == JobSource::TEMP_CONTROL);
    // 같은 방향으로 이어지면 커진다: 5.0 → 7.5
    v[0].pos = v[1].pos = 1500;
    t += 60000;
    n = tc.tick(t, true, 300, false, v, jobs);
    CHECK(n == 2 && jobs[0].target == 2250);
    // 메뉴 중에는 멈춤, 나오면 처음 크기
    v[0].pos = v[1].pos = 2250;
    t += 60000;
    CHECK(tc.tick(t, true, 300, true, v, jobs) == 0);
    t += 1000;
    CHECK(tc.tick(t, true, 300, false, v, jobs) == 0);
    t += 60000;
    n = tc.tick(t, true, 300, false, v, jobs);
    CHECK(n == 2 && jobs[0].target == 2750);
    // 낮으면 닫는 쪽, 0.0초에서 한 번 더 낮으면 CLOSE
    v[0].pos = v[1].pos = 0;
    t += 60000;
    n = tc.tick(t, true, 260, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::CLOSE);
    // CLOSE에서는 불감대 안으로 들어오면 0.0초로
    v[0].posState = v[1].posState = PosState::CLOSED;
    t += 60000;
    n = tc.tick(t, true, 276, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::MOVE && jobs[0].target == 0);
    // 고장 창에는 넣지 않는다
    v[1].eligible = false;
    t += 60000;
    n = tc.tick(t, true, 276, false, v, jobs);
    CHECK(n == 1 && jobs[0].window == 0);
}

// ------------------------------------------------------------
//  Dispatcher: 가짜 개폐기 두 대와 릴레이 전압
// ------------------------------------------------------------
struct Plant {
    double u = 0, top = 20000, sensorAt = 1000;
    int32_t current(int8_t dir, uint16_t duty) const {
        if (dir == 0 || duty == 0) return 0;
        if (dir > 0 && u >= top) return 0;
        if (dir < 0 && u <= 0) return 0;
        return 2000 * (duty < 200 ? 200 : duty) / 1000 + 400;
    }
    void step(int8_t dir, uint16_t duty) {
        double d = duty / 1000.0;
        if (dir > 0) { u += d; if (u > top) u = top; }
        if (dir < 0) { u -= d; if (u < 0) u = 0; }
    }
};

struct DSim {
    Dispatcher d;
    Plant p[2];
    ControlOutputs out = {};
    uint32_t now = 0;
    int32_t estopMv = 24000;
    int32_t tempUa = 0;
    bool relayStuck = false;
    void step() {
        ControlInputs in = {};
        in.now = now;
        for (int i = 0; i < 2; i++) {
            in.currentMa[i] = p[i].current(out.dir[i], out.duty[i]);
            in.sensor[i] = p[i].u < p[i].sensorAt ? SensorState::PRESENT : SensorState::ABSENT;
        }
        in.relayMv = (out.fetA && out.fetB) || relayStuck ? 24000 : 0;
        in.estopMv = estopMv;
        in.tempUa = tempUa;
        out = d.step(in);
        for (int i = 0; i < 2; i++) p[i].step(out.dir[i], out.duty[i]);
        now += CONTROL_PERIOD_MS;
    }
    void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += CONTROL_PERIOD_MS) step(); }
};

static Settings baseSettings() {
    Settings s;
    SettingsRules::defaults(s);
    for (int i = 0; i < 2; i++) {
        WindowSettings& w = s.win[i];
        w.upTime = 19000; w.downTime = 19000; w.overlap = 1000; w.backoff = 150;
        w.totalUp = 20000; w.iMaxUp = 2500; w.iMaxDn = 2500;
        SettingsRules::recomputeHomeTime(w);
    }
    return s;
}

static void test_dispatcher_boot_and_move() {
    DSim s;
    Settings st = baseSettings();
    s.tempUa = 12000;
    s.d.begin(Model::W50, st, 0);
    Job j = {};
    j.type = JobType::MOVE; j.source = JobSource::ENCODER; j.window = 0; j.target = 5000;
    CHECK(!s.d.job(j, s.now));                               // 점검 전에는 받지 않음
    s.run(2100);
    CHECK(s.d.bootDone());
    Status status;
    s.d.fillStatus(status);
    CHECK(status.win[0].sensorFound && status.win[1].sensorFound);   // 하한에 있어 입력이 ON
    CHECK(status.temp.found && status.temp.valid && status.temp.temp == 250);
    CHECK(!status.relayFault);

    // 메뉴 안에서는 온도제어가 멈춘다 (설정 온도 28.0, 현재 25.0이라 원래는 CLOSE를 넣음)
    Alert menu = { AlertType::MENU_ENTER, 0xFF, Cause::NONE };
    s.d.alert(menu, s.now);
    CHECK(s.d.job(j, s.now));
    s.run(80000);
    s.d.fillStatus(status);
    CHECK(status.win[0].state == WinState::IDLE);
    CHECK(status.win[0].posState == PosState::VALID);
    CHECK_NEAR(status.win[0].pos, 5000, 5);
    CHECK(status.win[1].posState == PosState::UNKNOWN);      // 온도제어가 움직이지 않음
    CHECK(status.temp.paused);
    CHECK(s.out.fetA == false && s.out.fetB == false);       // 멈추면 릴레이를 뗀다
}

static void test_dispatcher_estop_and_relay() {
    DSim s;
    Settings st = baseSettings();
    st.temp.sensorAuto = false;
    s.d.begin(Model::W50, st, 0);
    s.run(3100);
    Job j = {};
    j.type = JobType::MOVE; j.source = JobSource::ENCODER; j.window = 1; j.target = 8000;
    CHECK(s.d.job(j, s.now));
    s.run(5000);
    Status status;
    s.d.fillStatus(status);
    CHECK(isMovingState(status.win[1].state));
    s.estopMv = 0;
    s.run(50);
    s.d.fillStatus(status);
    CHECK(status.estop && status.win[1].state == WinState::ESTOP);
    CHECK(s.out.dir[1] == 0);
    s.estopMv = 24000;
    s.run(50);
    s.d.fillStatus(status);
    CHECK(!status.estop && status.win[1].state == WinState::IDLE);   // 풀어도 스스로 움직이지 않음
    s.run(2000);
    s.d.fillStatus(status);
    CHECK(status.win[1].state == WinState::IDLE);

    // 릴레이 접점 붙음: 전원 켤 때 점검에서 고장, 두 창 모두 막힘, 이벤트는 한 번
    DSim r;
    r.relayStuck = true;
    r.d.begin(Model::W300, st, 0);
    r.run(3100);
    r.d.fillStatus(status);
    CHECK(status.relayFault);
    CHECK(status.win[0].state == WinState::FAULT && status.win[0].fault == Cause::RELAY);
    CHECK(status.win[1].state == WinState::FAULT);
    CHECK(status.eventCount == 1 && status.events[0].window == 0 && status.events[0].cause == Cause::RELAY);
    Alert clr = { AlertType::CLEAR_FAULT, 0xFF, Cause::RELAY };
    r.d.alert(clr, r.now);
    r.run(100);
    r.d.fillStatus(status);
    CHECK(status.win[0].state == WinState::FAULT);           // 릴레이 고장은 남는다
}

static void test_dispatcher_temp_lost() {
    DSim s;
    Settings st = baseSettings();
    s.tempUa = 12000;
    s.d.begin(Model::W50, st, 0);
    s.run(3100);
    s.tempUa = 500;                                           // 끊김
    s.run(900);
    Status status;
    s.d.fillStatus(status);
    CHECK(status.temp.valid);                                 // 1초 전에는 그대로
    s.run(200);
    s.d.fillStatus(status);
    CHECK(!status.temp.valid);
    CHECK(status.eventCount >= 1 && status.events[(status.eventCount - 1) % EVENT_RING_SIZE].cause == Cause::TEMP_LOST);
    s.tempUa = 12000;
    s.run(100);
    s.d.fillStatus(status);
    CHECK(status.temp.valid);
}

// ------------------------------------------------------------
//  UI 시험 빌드의 가짜 개폐기 (dev_Sim.h)로 실제 흐름이 끝까지 도는지
// ------------------------------------------------------------
static void test_uisim_plant() {
    Dispatcher d;
    Settings st;
    SettingsRules::defaults(st);                              // 빈 설정: 처음 켠 보드와 같음
    SimWindow w[2];
    bool fetA = false, fetB = false;
    ControlOutputs out = {};
    uint32_t now = 0;
    d.begin(Model::W50, st, now);
    auto run = [&](uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += CONTROL_PERIOD_MS) {
            ControlInputs in = {};
            in.now = now;
            for (int i = 0; i < 2; i++) {
                w[i].step(out.dir[i], out.duty[i], CONTROL_PERIOD_MS);
                in.currentMa[i] = w[i].currentMa(out.dir[i], out.duty[i]);
                in.sensor[i] = w[i].sensorOn() ? SensorState::PRESENT : SensorState::ABSENT;
            }
            in.relayMv = (fetA && fetB) ? SIM_24V_MV : 0;
            in.estopMv = SIM_24V_MV;
            in.tempUa = SIM_TEMP_UA;
            out = d.step(in);
            fetA = out.fetA; fetB = out.fetB;
            now += CONTROL_PERIOD_MS;
        }
    };
    run(3000);
    CHECK(d.bootDone());
    Status status;
    d.fillStatus(status);
    CHECK(!status.relayFault && !status.estop);
    CHECK(status.temp.valid && status.temp.temp == 250);

    Job j = {};
    j.type = JobType::CALIBRATE; j.source = JobSource::ENCODER; j.window = 0;
    CHECK(d.job(j, now));
    run(150000);
    d.fillStatus(status);
    CHECK(status.win[0].cal.phase == CalPhase::DONE);
    CHECK(!status.win[0].cal.outOfRange);
    CHECK_NEAR(status.win[0].cal.totalUp, SIM_TRAVEL_CS, 3);
    CHECK(status.win[0].state == WinState::IDLE);
    CHECK(w[0].pos() == 0);
}

int main() {
    RUN(test_format_values);
    RUN(test_format_lines);
    RUN(test_format_temp_screen);
    RUN(test_error_log);
    RUN(test_sensor_filter);
    RUN(test_temp_control);
    RUN(test_dispatcher_boot_and_move);
    RUN(test_dispatcher_estop_and_relay);
    RUN(test_dispatcher_temp_lost);
    RUN(test_uisim_plant);
    return finish();
}
