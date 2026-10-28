// 화면 글자, 에러 저장, 센서 판정, 온도제어, Dispatcher를 PC에서 시험한다.
#include <cstring>
#include "../minitest.h"
#include "../../io_Format.h"
#include "../../ErrorLog.h"
#include "../../ctl_Safety.h"
#include "../../app_TempControl.h"
#include "../../app_Dispatcher.h"
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

// ------------------------------------------------------------
static void test_format_values() {
    char b[24];
    Fmt::secs(b, 18234, 5); CHECK(eq(b, "182.5"));
    Fmt::secs(b, 18224, 5); CHECK(eq(b, "182.0"));
    Fmt::secs(b, 4500, 5);  CHECK(eq(b, " 45.0"));
    Fmt::secs(b, CS_EMPTY, 5); CHECK(eq(b, "---.-"));
    Fmt::secs2(b, 16352); CHECK(eq(b, "163.52"));
    Fmt::pct4(b, 496); CHECK(eq(b, "49.6"));
    Fmt::pct4(b, 5);   CHECK(eq(b, " 0.5"));
    Fmt::pct4(b, 999); CHECK(eq(b, "99.9"));
    Fmt::pct4(b, PCT_UNKNOWN); CHECK(eq(b, "--.-"));
    Fmt::amps(b, 2140); CHECK(eq(b, "2.1A"));
    Fmt::temp4(b, 255); CHECK(eq(b, "25.5"));
    Fmt::temp4(b, 50);  CHECK(eq(b, " 5.0"));
    Fmt::temp4(b, -150); CHECK(eq(b, "-9.9"));             // 표시만 -9.9
    Fmt::temp4(b, 1234); CHECK(eq(b, "99.9"));
    Fmt::buildDate(b, "Oct  9 2026", "19:47:12"); CHECK(eq(b, "2026-10-09 19:47"));
    Fmt::splashTop(b, "WIN CTRL", 0, 9); CHECK(eq(b, "WIN CTRL   00.09"));

    // 위치는 백분율로만 (스펙 3절): 0.0 CLOSE, 1.0 = 0.0초, 99.9 = 최대 위치
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 0, 18300)) == 10);
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 9000, 18300)) == 496);
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 18300, 18300)) == 999);
    CHECK(Fmt::winPct(idleWin(PosState::CLOSED, 0, 18300)) == PCT_CLOSE);
    CHECK(Fmt::winPct(idleWin(PosState::UNKNOWN, 0, 18300)) == PCT_UNKNOWN);
}

// 작은 글씨 화면 (스펙 7절)
static void test_format_small() {
    char l[24];
    WindowStatus w = idleWin(PosState::VALID, 9000, 18300);
    Fmt::smallLine(l, w, 2, true, false); CHECK(eq(l, "2 49.6          "));
    w = idleWin(PosState::CLOSED, 0, 18000);
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 CLOSE         "));
    w = idleWin(PosState::UNKNOWN, 0, 18000);
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 --.-         ?"));

    w = idleWin(PosState::VALID, 9000, 18300);
    w.state = WinState::MOVING; w.dir = 1; w.target = 18300;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 49.6>99.9    ^"));
    Fmt::smallLine(l, w, 1, false, false); CHECK(eq(l, "1 49.6>99.9     "));
    w.target = -1; w.dir = -1;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 49.6>CLOSE   v"));

    w.state = WinState::REVERSE_WAIT; w.dir = 0; w.nextDir = -1;
    Fmt::smallLine(l, w, 1, false, false); CHECK(l[15] == 'v');   // 전환 정지 중에는 켜 둔다

    w = idleWin(PosState::VALID, 18300, 18300);
    w.state = WinState::LIMIT;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 99.9         L"));

    w.state = WinState::FAULT; w.fault = Cause::OVERLOAD;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 ERR OVERLOAD !"));

    w = idleWin(PosState::UNKNOWN, 0, 18300);
    w.state = WinState::HOMING; w.dir = -1; w.target = 9000;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 HOME >49.6   v"));

    w.state = WinState::CALIBRATING; w.dir = 1; w.cal.elapsed = 6350;
    Fmt::smallLine(l, w, 1, true, false); CHECK(eq(l, "1 CAL  63.5s   ^"));

    w = idleWin(PosState::VALID, 9000, 18300);
    Fmt::smallLine(l, w, 1, true, true); CHECK(eq(l, "1 ERR SENSOR    "));
}

// 큰 글씨와 가로 두 배 숫자
static void test_format_big() {
    char r0[17], r1[17];
    std::memset(r0, ' ', 16); std::memset(r1, ' ', 16); r0[16] = r1[16] = 0;
    int8_t dec = Fmt::bigNumber(r0, r1, 287);
    // 2: " UR" / "LB ", 8: "LUR" / "LBR" (U 위아래, B 아래 가로획), 아랫줄 소수점과 소수 자리 반쪽 둘
    const char want0[10] = { ' ', Fmt::G_BOTH, Fmt::G_VR, Fmt::G_VL, Fmt::G_BOTH, Fmt::G_VR, ' ', ' ', ' ', 0 };
    const char want1[10] = { Fmt::G_VL, Fmt::G_BOT, ' ', Fmt::G_VL, Fmt::G_BOT, Fmt::G_VR, Fmt::G_DOT,
                             Fmt::G_DEC, (char)(Fmt::G_DEC + 1), 0 };
    CHECK(dec == 7);
    CHECK(std::memcmp(r0, want0, 9) == 0);
    CHECK(std::memcmp(r1, want1, 9) == 0);
    CHECK(r0[10] == ' ' && r1[10] == ' ');                      // 오른쪽은 건드리지 않음
    // 소수 자리는 가로 두 배 숫자를 한 줄 내린 것
    uint8_t D[8], W[8];
    Fmt::bigDecHalf(7, 0, D); Fmt::wideHalf(7, 0, W);
    CHECK(D[0] == 0 && std::memcmp(D + 1, W, 7) == 0);
    Fmt::bigNumber(r0, r1, 55);                                 // 10 아래는 앞자리를 비운다
    CHECK(r0[0] == ' ' && r0[1] == ' ' && r0[2] == ' ' && r1[0] == ' ');
    CHECK(Fmt::bigNumber(r0, r1, PCT_UNKNOWN) == -1);
    CHECK(r0[1] == Fmt::G_BOT && r0[0] == ' ' && r1[1] == ' ');
    Fmt::bigDecHalf(-1, 0, D);                                  // 모름: 소수 자리도 가운데 획만
    CHECK(D[4] == 0x0F && D[1] == 0 && D[7] == 0);

    // 가로 두 배 숫자: 9x7 점, 맨 왼쪽 줄과 맨 아래 줄은 빈다
    for (uint8_t d = 0; d < 10; d++) {
        uint8_t L[8], R[8];
        Fmt::wideHalf(d, 0, L); Fmt::wideHalf(d, 1, R);
        bool ok = L[7] == 0 && R[7] == 0;
        for (uint8_t y = 0; y < 8; y++) if (L[y] & 0x10) ok = false;
        CHECK(ok);
    }
    uint8_t L[8], R[8];
    Fmt::wideHalf(8, 0, L); Fmt::wideHalf(8, 1, R);
    CHECK(L[0] == 0x0F && R[0] == 0x1F);                        // 가로획 1점 (윗줄 9점)
    CHECK(L[1] == 0x0C && R[1] == 0x03);                        // 세로획 2점

    // 숫자 칸 나누기: 이미 있는 숫자는 그 칸에 둔다
    Fmt::WideSlots s = { { -1, -1, -1, -1 } };
    bool need[10] = {};
    Fmt::wideNeed(need, 496); Fmt::wideNeed(need, 999);
    Fmt::wideAssign(s, need);
    CHECK(s.digit[0] == 4 && s.digit[1] == 9 && s.digit[2] == -1);
    bool need2[10] = {};
    Fmt::wideNeed(need2, 355); Fmt::wideNeed(need2, 999);
    Fmt::wideAssign(s, need2);
    CHECK(s.digit[1] == 9 && (s.digit[0] == 3 || s.digit[2] == 3) && (s.digit[0] == 5 || s.digit[2] == 5));

    char l[17];
    Fmt::WideSlots t = { { 4, 9, -1, -1 } };
    Fmt::wideLine(l, 1, 496, t, true, ">35.5 v");
    const char wl[17] = { '1', ' ', 8, 9, 10, 11, '.', '6', ' ', '>', '3', '5', '.', '5', ' ', 'v', 0 };
    CHECK(std::memcmp(l, wl, 16) == 0);
    Fmt::wideLine(l, 2, 50, t, true, "CLOSE");                    // 5.0: 앞자리 비움, 5는 칸이 없어 작게
    CHECK(l[2] == ' ' && l[3] == ' ' && l[4] == '5' && l[6] == '.' && l[7] == '0');
    Fmt::wideLine(l, 1, 496, t, false, nullptr);                  // 깜빡임: 값 칸이 빈다
    CHECK(eq(l, "1               "));
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
    s.run(RELAY_HOLD_MS + 500);
    CHECK(s.out.fetA == false && s.out.fetB == false);       // 멈추고 10초 지나면 MC를 뗀다
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
    s.run(200);
    CHECK(s.out.fetA == false && s.out.fetB == false);       // 비상정지는 유지 시간 없이 MC를 뗀다
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

// 위치 동기 잡(양쪽), LINK, 수동과 자동 (스펙 7절, 9절)
static void test_dispatcher_sync_and_modes() {
    DSim s;
    Settings st = baseSettings();
    st.win[1].link = false;                                  // 창2는 온도제어에서 빠짐
    s.tempUa = 12000;                                         // 25.0도, 설정 28.0도라 온도제어는 닫으려 함
    s.d.begin(Model::W50, st, 0);
    Alert man = { AlertType::MODE_MANUAL, 0xFF, Cause::NONE };
    s.d.alert(man, s.now);
    s.run(2100);
    CHECK(s.d.bootDone());

    Job j = {};
    j.type = JobType::MOVE; j.source = JobSource::ENCODER; j.window = JOB_BOTH;
    j.bothMask = 3; j.target = 5000; j.target2 = 8000;
    CHECK(s.d.job(j, s.now));
    s.step();
    Status status;
    s.d.fillStatus(status);
    CHECK(isMovingState(status.win[0].state) && isMovingState(status.win[1].state));   // 같은 주기에 둘 다
    s.run(150000);
    s.d.fillStatus(status);
    CHECK_NEAR(status.win[0].pos, 5000, 5);
    CHECK_NEAR(status.win[1].pos, 8000, 5);
    CHECK(status.temp.manual && status.temp.paused);
    s.run(70000);                                             // 수동이라 온도제어가 움직이지 않음
    s.d.fillStatus(status);
    CHECK_NEAR(status.win[0].pos, 5000, 5);

    // 양쪽 잡의 -1은 CLOSE, 마스크에 없는 창은 그대로
    j.bothMask = 1; j.target = -1; j.target2 = 0;
    CHECK(s.d.job(j, s.now));
    s.run(80000);
    s.d.fillStatus(status);
    CHECK(status.win[0].posState == PosState::CLOSED);
    CHECK_NEAR(status.win[1].pos, 8000, 5);

    // 자동: LINK가 켜진 창만 움직인다 (창1은 닫혀 있어 그대로, 창2는 LINK 꺼짐)
    j.bothMask = 1; j.target = 5000;
    CHECK(s.d.job(j, s.now));
    s.run(60000);
    Alert aut = { AlertType::MODE_AUTO, 0xFF, Cause::NONE };
    s.d.alert(aut, s.now);
    s.run(130000);
    s.d.fillStatus(status);
    CHECK(!status.temp.manual);
    CHECK(status.win[0].pos < 5000 || status.win[0].posState == PosState::CLOSED);
    CHECK_NEAR(status.win[1].pos, 8000, 5);
}

int main() {
    RUN(test_format_values);
    RUN(test_format_small);
    RUN(test_format_big);
    RUN(test_error_log);
    RUN(test_sensor_filter);
    RUN(test_temp_control);
    RUN(test_dispatcher_boot_and_move);
    RUN(test_dispatcher_estop_and_relay);
    RUN(test_dispatcher_temp_lost);
    RUN(test_dispatcher_sync_and_modes);
    return finish();
}
