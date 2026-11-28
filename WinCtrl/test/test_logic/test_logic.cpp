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

static WindowStatus idleWin(PosState ps, cs_t pos, cs_t maxPos, cs_t home = 0) {
    WindowStatus w = {};
    w.state = WinState::IDLE;
    w.posState = ps;
    w.pos = pos;
    w.home = home;
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
    Fmt::splashTop(b, "WIN CTRL", 0, 90); CHECK(eq(b, "WIN CTRL   00.90"));

    // 위치는 백분율로만 (스펙 3절): 0.0 CLOSE(bottom), 1.0 = home, 99.9 = top
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 2000, 15000, 2000)) == 10);
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 8000, 15000, 2000)) == 466);
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 0, 18300)) == PCT_CLOSE);   // home = bottom이면 CLOSE
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 9000, 18300)) == 496);
    CHECK(Fmt::winPct(idleWin(PosState::VALID, 18300, 18300)) == 999);
    CHECK(Fmt::winPct(idleWin(PosState::CLOSED, 0, 18300)) == PCT_CLOSE);
    CHECK(Fmt::winPct(idleWin(PosState::UNKNOWN, 0, 18300)) == PCT_UNKNOWN);
}

// 작은 글씨 화면 (스펙 7절)
static void test_format_small() {
    char l[24];
    WindowStatus w = idleWin(PosState::VALID, 9000, 18300);
    {
        char r0[17], r1[17];
        Fmt::notSetLines(r0, r1, 3); CHECK(eq(r0, "MOTOR NOT SET 12")); CHECK(eq(r1, "USE DEFAULT PUSH"));
        Fmt::notSetLines(r0, r1, 1); CHECK(eq(r0, "MOTOR NOT SET 1 "));
        Fmt::notSetLines(r0, r1, 2); CHECK(eq(r0, "MOTOR NOT SET  2"));
        // 기본 구성 경고 (2026-11-28)
        Fmt::basicLines(r0, r1, Fmt::BASIC_MC, 0, false);
        CHECK(eq(r0, "NO MAIN MC      ")); CHECK(eq(r1, "NO POWER CUT   >"));
        Fmt::basicLines(r0, r1, Fmt::BASIC_HOME, 3, false); CHECK(eq(r0, "NO HOME SENSOR  "));
        CHECK(eq(r1, "POS BY TIME    >"));
        Fmt::basicLines(r0, r1, Fmt::BASIC_HOME, 2, false); CHECK(eq(r0, "NO HOME SENS 2  "));
        Fmt::basicLines(r0, r1, Fmt::BASIC_TEMP, 0, true);
        CHECK(eq(r0, "NO TEMP SENSOR  ")); CHECK(eq(r1, "MANUAL ONLY   OK"));
    }
    Fmt::smallLine(l, w, 2, Cause::NONE); CHECK(eq(l, "2 49.6          "));
    w = idleWin(PosState::CLOSED, 0, 18000);
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 CLOSE         "));
    w = idleWin(PosState::UNKNOWN, 0, 18000);
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 --.-         ?"));

    w = idleWin(PosState::VALID, 9000, 18300);
    w.state = WinState::MOVING; w.dir = 1; w.target = 18300;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 49.6>99.9    ^"));
    w.target = -1; w.dir = -1;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 49.6>CLOSE   v"));

    w.state = WinState::REVERSE_WAIT; w.dir = 0; w.nextDir = -1;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(l[15] == 'v');          // 전환 정지 중에는 다음 방향

    w = idleWin(PosState::VALID, 18300, 18300);
    w.state = WinState::LIMIT;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 99.9         L"));

    w.state = WinState::FAULT; w.fault = Cause::OVERLOAD;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 ERR OVERLOAD !"));

    w = idleWin(PosState::UNKNOWN, 0, 18300);
    w.state = WinState::HOMING; w.dir = -1; w.target = 9000;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 HOME >49.6   v"));

    w.state = WinState::CALIBRATING; w.dir = 1; w.cal.elapsed = 6350;
    Fmt::smallLine(l, w, 1, Cause::NONE); CHECK(eq(l, "1 CAL  63.5s   ^"));

    w = idleWin(PosState::VALID, 9000, 18300);
    Fmt::smallLine(l, w, 1, Cause::HOME_SENSOR); CHECK(eq(l, "1 ERR SENSOR    "));
}

// 큰 글씨와 가로 두 배 숫자
static void test_format_big() {
    char r0[17], r1[17];
    std::memset(r0, ' ', 16); std::memset(r1, ' ', 16); r0[16] = r1[16] = 0;
    int8_t dec = Fmt::bigNumber(r0, r1, 287);
    // 2: " UR" / "LB ", 8: "LUR" / "LBR" (U 위아래, B 아래 가로획), 아랫줄 소수점과 소수 자리 반쪽 둘
    const char want0[10] = { ' ', Fmt::G_BOTH, Fmt::G_VR, Fmt::G_VL, Fmt::G_BOTH, Fmt::G_VR, ' ', ' ', ' ', 0 };
    const char want1[10] = { Fmt::G_VL, Fmt::G_BOT, ' ', Fmt::G_VL, Fmt::G_BOT, Fmt::G_VR, '.',
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
    Fmt::wideLine(l, 1, 496, t, false, ">35.5 v");
    const char wl[17] = { '1', ' ', 8, 9, 10, 11, '.', '6', ' ', '>', '3', '5', '.', '5', ' ', 'v', 0 };
    CHECK(std::memcmp(l, wl, 16) == 0);
    Fmt::wideLine(l, 2, 50, t, false, "CLOSE");                   // 5.0: 앞자리 비움, 5는 칸이 없어 작게
    CHECK(l[2] == ' ' && l[3] == ' ' && l[4] == '5' && l[6] == '.' && l[7] == '0');
    Fmt::wideLine(l, 1, 496, t, true, nullptr);                   // 고른 창: 번호 옆에 *
    CHECK(l[0] == '1' && l[1] == '*' && l[2] == 8 && l[7] == '6');
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
    for (int i = 0; i < 2; i++) { v[i].eligible = true; v[i].posState = PosState::VALID; v[i].pos = 1000; v[i].home = 0; v[i].limit = 18000; }
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
    // 낮으면 닫는 쪽: home 아래로는 가지 않는다
    v[0].home = v[1].home = 2000;
    v[0].pos = v[1].pos = 2250;
    t += 60000;
    n = tc.tick(t, true, 260, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::MOVE && jobs[0].target == 2000);
    // home에서 한 번 더 낮으면 CLOSE
    v[0].pos = v[1].pos = 2000;
    t += 60000;
    n = tc.tick(t, true, 260, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::CLOSE);
    v[0].home = v[1].home = 0;
    // home = bottom인 창도 같다
    v[0].pos = v[1].pos = 0;
    t += 60000;
    n = tc.tick(t, true, 260, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::CLOSE);
    // CLOSE에서는 불감대 안으로 들어오면 home으로
    v[0].posState = v[1].posState = PosState::CLOSED;
    t += 60000;
    n = tc.tick(t, true, 276, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::MOVE && jobs[0].target == 0);
    // 고장 창에는 넣지 않는다
    v[1].eligible = false;
    t += 60000;
    n = tc.tick(t, true, 276, false, v, jobs);
    CHECK(n == 1 && jobs[0].window == 0);
    // 설정 온도를 바꾸면 주기를 기다리지 않고 바로 판단한다
    v[1].eligible = true;
    v[0].posState = v[1].posState = PosState::VALID;
    v[0].pos = v[1].pos = 1000;
    t += 60000;
    tc.tick(t, true, 276, false, v, jobs);                   // 방금 판단함
    t += 1000;
    CHECK(tc.tick(t, true, 276, false, v, jobs) == 0);       // 주기 전: 판단 없음
    ts.setT = 260;
    tc.setSettings(ts);
    tc.decideNow();
    t += 1000;
    n = tc.tick(t, true, 276, false, v, jobs);
    CHECK(n == 2 && jobs[0].type == JobType::MOVE && jobs[0].target > 1000);
    t += 1000;
    CHECK(tc.tick(t, true, 276, false, v, jobs) == 0);       // 한 번만
    // 멈춤 중에 바꾸면 풀린 뒤 첫 tick에서
    tc.decideNow();
    t += 1000;
    CHECK(tc.tick(t, true, 276, true, v, jobs) == 0);
    t += 1000;
    CHECK(tc.tick(t, true, 276, false, v, jobs) == 2);
}

// ------------------------------------------------------------
//  온도 흐름 역전과 반응 시간으로 판단 주기 줄이기 (2026-11-28)
// ------------------------------------------------------------
// 하우스 모형: 해가 데우고(분당 heat), 창을 연 만큼 delay 뒤부터 식는다. 창은 잡을 받으면 바로 그 자리로
struct House {
    TempControl tc;
    TempSettings t;
    TempControl::WinView v[2];
    double temp = 280;            // 0.1도
    double heat = 6;              // 분당 0.1도 단위
    double cool = 0.002;          // 1cs 열림당 분당 식는 양 (5초 열면 분당 1.0도)
    uint32_t delayMs = 90000;
    uint32_t now = 0;
    cs_t hist[4000] = {};         // 10ms마다가 아니라 1초마다 위치 기록
    uint32_t jobs = 0, lastJobMs = 0;
    House() {
        SettingsRules::defaultsTemp(t);
        t.sensorAuto = true; t.setT = 280; t.deadband = 5; t.periodS = 300; t.aheadMin = 0; t.slopeMin = 2;
        tc.setSettings(t);
        tc.begin(0);
        for (int i = 0; i < 2; i++) {
            v[i].eligible = true; v[i].posState = PosState::VALID; v[i].pos = 1000; v[i].home = 1000; v[i].limit = 15000;
        }
    }
    void run(uint32_t ms) {
        for (uint32_t e = 0; e < ms; e += 1000) {
            uint32_t sec = now / 1000;
            hist[sec % 4000] = v[0].pos;
            uint32_t back = delayMs / 1000;
            cs_t felt = sec >= back ? hist[(sec - back) % 4000] : 1000;
            temp += (heat - cool * (felt - 1000) * 10) / 60.0;
            Job j[2];
            uint8_t n = tc.tick(now, true, (int16_t)(temp + 0.5), false, v, j);
            for (uint8_t k = 0; k < n; k++) {
                TempControl::WinView& w = v[j[k].window];
                if (j[k].type == JobType::CLOSE) { w.pos = 0; w.posState = PosState::CLOSED; }
                else { w.pos = j[k].target; w.posState = PosState::VALID; }
            }
            if (n) { jobs++; lastJobMs = now; }
            now += 1000;
        }
    }
};

// 우리가 연 효과로 오르던 온도가 내려가기 시작하면: 더 열지 않고 주기를 다시 센다
static void test_trend_hold_after_our_move() {
    House h;
    h.temp = 286; h.heat = 6;                 // 더워지는 중
    h.run(301000);                            // 300초에 첫 판단: 연다
    CHECK(h.jobs == 1);
    uint32_t moved = h.lastJobMs;
    uint16_t rev0 = h.tc.reversalCount();
    // 연 효과가 90초 뒤부터: 오르던 흐름이 내림으로
    for (int i = 0; i < 400 && h.tc.reversalCount() == rev0; i++) h.run(1000);
    CHECK(h.tc.reversalCount() == rev0 + 1);
    CHECK(h.tc.lastReversalHeld());           // 우리 효과
    CHECK(h.tc.trend() < 0);
    CHECK(h.tc.responseCount() == 1);
    CHECK(h.tc.lastResponseS() >= 90 && h.tc.lastResponseS() <= 300);
    uint32_t revAt = h.now;
    CHECK(h.jobs == 1);                        // 역전 때 더 움직이지 않음
    // 다음 판단은 역전에서 주기만큼 뒤 (움직인 때부터 세지 않음)
    h.run(299000 - 2000);
    CHECK(h.lastJobMs == moved);
    (void)revAt;
}

// 움직이지 않았는데 흐름이 거꾸로 바뀜(해가 남): 열원 변화는 아직 다루지 않으므로 주기대로 판단
static void test_trend_outside_change_waits_period() {
    House h;
    h.t.periodS = 600; h.tc.setSettings(h.t);
    h.temp = 284; h.heat = -0.8;              // 불감대 안에서 천천히 내려감 (10분에 0.8도)
    h.run(610000);                            // 600초 판단: 불감대 안이라 움직이지 않음
    CHECK(h.jobs == 0);
    CHECK(h.tc.trend() < 0);
    h.heat = 12;                              // 해가 남: 빠르게 오름
    uint16_t rev0 = h.tc.reversalCount();
    h.run(589000);
    CHECK(h.tc.reversalCount() == rev0 + 1);
    CHECK(!h.tc.lastReversalHeld());
    CHECK(h.jobs == 0);                       // 1200초 주기 전에는 움직이지 않음
    h.run(2000);
    CHECK(h.jobs == 1);                       // 주기에 판단
}

// 반응이 빠르면(바람) 판단 주기가 짧아지고, 느리면 설정값 그대로
static void test_period_follows_response() {
    House fast;
    fast.delayMs = 40000; fast.temp = 290; fast.heat = 8;
    fast.run(3 * 3600 * 1000);
    CHECK(fast.tc.responseCount() >= 2);
    uint16_t r = fast.tc.responseS();
    CHECK(r > 0 && r < 300);
    CHECK(fast.tc.periodMs() < 300000);                     // 설정 300초보다 짧아짐
    CHECK(fast.tc.periodMs() >= (uint32_t)TEMP_PERIOD_MIN_S * 1000);
    CHECK(fast.tc.periodMs() == (r * 1200u < 30000u ? 30000u : r * 1200u) || fast.tc.periodMs() == 300000);

    House none;                                              // 측정이 없으면 설정값
    CHECK(none.tc.responseS() == 0 && none.tc.periodMs() == 300000);

    // 손으로 움직이면 그 측정은 버린다
    House m;
    m.temp = 286;
    m.run(301000);
    CHECK(m.jobs == 1);
    m.tc.resetStep();
    m.run(600000);
    CHECK(m.tc.responseCount() == 0 || m.tc.lastResponseS() < 400);
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
        // bottom → top 200초, 센서 지점 10초, 되내림 1.5초 → 원점 시간 8.5초
        w.upTime = 20000; w.downTime = 20000; w.backoff = 150; w.sensorRun = 1000;
        w.iMaxUp = 3250; w.iMaxDn = 3250; w.homeSensAuto = true;
        SettingsRules::recomputeHomeTime(w);
    }
    s.relayUse = true;            // 이 시험들은 MC와 온도 센서가 있는 구성
    s.temp.sensorAuto = true;
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

    // MC 접점 융착: 경고만 내고 두 창 모두 그대로 운전한다. 시도할 때마다 경고 (2026-11-28)
    DSim r;
    r.relayStuck = true;
    r.d.begin(Model::W300, st, 0);
    r.run(3100);
    r.d.fillStatus(status);
    CHECK(status.relayFault);
    CHECK(status.win[0].state != WinState::FAULT && status.win[1].state != WinState::FAULT);
    CHECK(status.eventCount == 1 && status.events[0].window == 0 && status.events[0].cause == Cause::RELAY);
    Job mv = {};
    mv.type = JobType::MOVE; mv.source = JobSource::ENCODER; mv.window = 0; mv.target = 3000;
    CHECK(r.d.job(mv, r.now));
    r.run(3000);
    CHECK(r.out.dir[0] != 0);                                // 구동한다
    r.d.fillStatus(status);
    CHECK(status.eventCount >= 2 && status.events[1].cause == Cause::RELAY);   // 이번 시도의 경고

    // MC가 붙지 않음(24V가 안 감): 경고를 내고 구동은 시도한다. 전류가 없으니 무전류 경고로 끝난다
    struct NoEngage : DSim {
        void stepNo() {
            ControlInputs in = {};
            in.now = now;
            in.relayMv = 0;
            in.estopMv = 24000;
            out = d.step(in);
            now += CONTROL_PERIOD_MS;
        }
    } ne;
    ne.d.begin(Model::W300, st, 0);
    for (int i = 0; i < 310; i++) ne.stepNo();
    Job mv2 = mv;
    CHECK(ne.d.job(mv2, ne.now));
    bool drove = false;
    for (int i = 0; i < 1500; i++) { ne.stepNo(); if (ne.out.dir[0] != 0) drove = true; }
    CHECK(drove);
    ne.d.fillStatus(status);
    bool relayEv = false, noCurr = false;
    for (uint32_t k = 0; k < status.eventCount && k < EVENT_RING_SIZE; k++) {
        if (status.events[k].cause == Cause::RELAY) relayEv = true;
        if (status.events[k].cause == Cause::NO_CURRENT) noCurr = true;
    }
    CHECK(relayEv && noCurr);
    CHECK(status.win[0].state != WinState::FAULT);
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

// ------------------------------------------------------------
//  UI 시험 빌드의 가짜 개폐기 (dev_Sim.h)로 실제 흐름이 끝까지 도는지
// ------------------------------------------------------------
static void test_uisim_plant() {
    Dispatcher d;
    Settings st;
    SettingsRules::defaults(st);                              // 빈 설정: 처음 켠 보드와 같음
    st.relayUse = true;                                       // UI 시험 빌드는 가짜 MC와 온도 센서가 있음
    st.temp.sensorAuto = true;
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
    CHECK_NEAR(status.win[0].cal.upTime, SIM_TRAVEL_CS, 3);
    CHECK(status.win[0].state == WinState::IDLE);
    CHECK(w[0].pos() == 0);
}

// 기본 구성 (2026-11-28): 설정이 없으면 MC 없음, 원점 센서 없음, 온도 센서 없음
static void test_basic_config() {
    Settings st;
    SettingsRules::defaults(st);
    CHECK(!st.relayUse && !st.basicAcked);
    CHECK(!st.win[0].homeSensAuto && !st.win[1].homeSensAuto && !st.temp.sensorAuto);
    CHECK(SettingsRules::valid(st));

    // MC 없음: 자가 점검 없이 2초 뒤 부팅 끝, FET는 켜지 않고 MC 뒤 전압이 없어도 구동한다
    DSim s;
    s.tempUa = 12000;                                          // 센서가 달려 있어도 끔이면 쓰지 않음
    s.p[0].u = 2000;                                           // 반쯤 열린 창
    s.d.begin(Model::W50, st, 0);
    s.run(TEMP_DETECT_MS + 20);
    CHECK(s.d.bootDone());
    Status status;
    s.d.fillStatus(status);
    CHECK(!status.relayUse && !status.relayFault && !status.temp.found);
    Job j = {};
    j.type = JobType::CLOSE; j.source = JobSource::ENCODER; j.window = 0;
    CHECK(s.d.job(j, s.now));
    s.run(2000);
    CHECK(s.out.dir[0] < 0);
    CHECK(!s.out.fetA && !s.out.fetB);
    s.run(30000);
    s.d.fillStatus(status);
    CHECK(status.win[0].posState == PosState::CLOSED && status.win[0].state == WinState::IDLE);
    CHECK(status.eventCount == 0);                              // MC 고장이 나지 않음

    // MC 뒤 전압이 떠 있어도(GPIO6 미결선) 고장이 아님
    DSim f;
    f.relayStuck = true;
    f.d.begin(Model::W50, st, 0);
    f.run(3000);
    f.d.fillStatus(status);
    CHECK(!status.relayFault && status.win[0].state != WinState::FAULT);

    // 메뉴에서 온도 센서를 켜면 다시 켜지 않아도 지금 전류로 판단한다
    Settings on = st;
    on.temp.sensorAuto = true;
    s.d.settingsChanged(on, 0, 0);
    s.run(100);
    s.d.fillStatus(status);
    CHECK(status.temp.found && status.temp.valid && status.temp.temp == 250);
    // 센서가 없는 채로 켜면 없음
    DSim n;
    n.d.begin(Model::W50, st, 0);
    n.run(2500);
    n.d.settingsChanged(on, 0, 0);
    n.run(100);
    n.d.fillStatus(status);
    CHECK(!status.temp.found);

    // MC 있음으로 바꾸면 자가 점검부터 하고, 그동안은 구동하지 않는다
    Settings mc = on;
    mc.relayUse = true;
    s.d.settingsChanged(mc, 0, 0);
    j.type = JobType::MOVE; j.window = 1; j.target = 3000;
    CHECK(s.d.job(j, s.now));
    s.run(100);
    CHECK(s.out.dir[1] == 0);
    s.run(3000);
    s.d.fillStatus(status);
    CHECK(status.relayUse && !status.relayFault);
    CHECK(s.out.fetA && s.out.fetB && s.out.dir[1] != 0);
}

// 저장 모양 버전 (2026-11-28): 지금 버전은 그대로, 3은 옮겨 읽고, 모르는 버전은 기본값
static void test_settings_load() {
    Settings cur = baseSettings();
    cur.basicAcked = true;
    Settings out;
    CHECK(SettingsRules::load(&cur, sizeof(cur), out) == SettingsRules::LOAD_OK);
    CHECK(out.relayUse && out.basicAcked && out.win[0].upTime == 20000);
    CHECK(SettingsRules::load(&cur, sizeof(cur) - 1, out) == SettingsRules::LOAD_NONE);

    struct V3 { uint16_t version; WindowSettings win[2]; TempSettings temp; bool clickSound; bool alarmSound; };
    V3 old;
    memset(&old, 0, sizeof(old));
    old.version = 3;
    old.win[0] = cur.win[0]; old.win[1] = cur.win[1];
    old.temp = cur.temp; old.temp.setT = 255;
    old.clickSound = false; old.alarmSound = true;
    Settings m;
    CHECK(SettingsRules::load(&old, sizeof(old), m) == SettingsRules::LOAD_MIGRATED);
    CHECK(m.version == SETTINGS_VERSION);
    CHECK(m.win[0].upTime == 20000 && m.win[1].homeSensAuto && m.temp.setT == 255 && m.temp.sensorAuto);
    CHECK(!m.clickSound && m.alarmSound);
    CHECK(!m.relayUse && !m.basicAcked);                       // 새 항목은 기본 구성 값

    old.version = 2;
    CHECK(SettingsRules::load(&old, sizeof(old), m) == SettingsRules::LOAD_NONE);
    Settings newer = cur;
    newer.version = SETTINGS_VERSION + 1;                      // 이 펌웨어보다 새 모양
    CHECK(SettingsRules::load(&newer, sizeof(newer), m) == SettingsRules::LOAD_NONE);
    Settings bad = cur;
    bad.temp.deadband = 0;                                     // 범위 밖
    CHECK(SettingsRules::load(&bad, sizeof(bad), m) == SettingsRules::LOAD_NONE);
    CHECK(SettingsRules::load(nullptr, 0, m) == SettingsRules::LOAD_NONE);
}

// 상태 LED: 부팅 중 계속 켜짐, 정상은 1초에 0.1초, 에러는 0.2초 간격 (스펙 7절)
static void test_status_led() {
    Dispatcher d;
    Settings st;
    SettingsRules::defaults(st);
    d.begin(Model::W50, st, 0);
    CHECK(d.statusLedOn(0) && d.statusLedOn(550));           // 부팅 중
    uint32_t now = 0;
    for (; now < 3000; now += CONTROL_PERIOD_MS) {
        ControlInputs in = {};
        in.now = now;
        in.estopMv = SIM_24V_MV;
        in.sensor[0] = in.sensor[1] = SensorState::ABSENT;
        d.step(in);
    }
    CHECK(d.bootDone());
    CHECK(d.statusLedOn(5000) && d.statusLedOn(5090));
    CHECK(!d.statusLedOn(5100) && !d.statusLedOn(5550) && !d.statusLedOn(5999));
    d.setUnackedError(true);
    CHECK(d.statusLedOn(5200) && d.statusLedOn(5399));
    CHECK(!d.statusLedOn(5400) && !d.statusLedOn(5599));
    CHECK(d.statusLedOn(5600));
    d.setUnackedError(false);
    CHECK(!d.statusLedOn(5600));
}

// 지그재그 취소 판정 (+3 -3 +3, 1초 안)
static bool zigRun(Fmt::Zigzag& z, const int8_t* seq, uint8_t n, uint32_t t0, uint32_t dt) {
    bool hit = false;
    for (uint8_t i = 0; i < n; i++) hit |= z.feed(seq[i], t0 + i * dt);
    return hit;
}
static void test_zigzag() {
    const int8_t ok[]    = { 1,1,1, -1,-1,-1, 1,1,1 };
    const int8_t two[]   = { 1,1, -1,-1, 1,1, -1,-1, 1,1 };       // 2칸씩은 아님
    const int8_t pair[]  = { 1,1,1, -1,-1,-1 };                   // 묶음 2개는 아님
    const int8_t fix[]   = { 1,1,1,1,1,1,1,1, -1 };               // 넘쳐서 한 칸 되돌림
    const int8_t back[]  = { 1,1,1,1, -1,-1,-1 };                 // 넘쳐서 되돌림 (묶음 2개)
    const int8_t lead[]  = { 1, -1, 1,1,1, -1,-1,-1, 1,1,1 };     // 앞의 튐은 무시
    const int8_t neg[]   = { -1,-1,-1, 1,1,1, -1,-1,-1 };         // - 로 시작해도 같음
    Fmt::Zigzag z;
    CHECK(zigRun(z, ok, 9, 1000, 120));          // 0.96초
    z.reset(); CHECK(!zigRun(z, ok, 9, 1000, 130));    // 1.04초: 늦음
    z.reset(); CHECK(!zigRun(z, two, 10, 1000, 50));
    z.reset(); CHECK(!zigRun(z, pair, 6, 1000, 50));
    z.reset(); CHECK(!zigRun(z, fix, 9, 1000, 50));
    z.reset(); CHECK(!zigRun(z, back, 7, 1000, 50));
    z.reset(); CHECK(zigRun(z, lead, 11, 1000, 50));
    z.reset(); CHECK(zigRun(z, neg, 9, 1000, 120));
    // 한 번 맞으면 처음부터: 바로 이어서 묶음 2개만 해도 안 걸림
    z.reset(); CHECK(zigRun(z, ok, 9, 1000, 50)); CHECK(!zigRun(z, pair, 6, 2000, 50));
}

static void test_auto_state() {
    // 이동이 끝날 때까지 남은 초
    WindowStatus w = {};
    w.state = WinState::IDLE;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == -1);
    w.state = WinState::MOVING; w.posState = PosState::VALID; w.dir = 1; w.pos = 2000; w.target = 3250;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == 13);           // 12.5초 → 올림
    w.dir = -1; w.target = 1000;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == 8);            // 10초 × 하강/상승 = 8초
    w.state = WinState::CLOSING; w.target = -1;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == 16);           // bottom까지 20초 × 0.8
    w.state = WinState::REVERSE_WAIT; w.nextDir = 1; w.dir = 0; w.target = 2500;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == 6);            // 5초 + 정지 1초
    w.state = WinState::HOMING;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == -2);
    w.state = WinState::CALIBRATING;
    CHECK(Fmt::moveRemainS(w, 10000, 8000) == -1);
    CHECK(std::strcmp(Fmt::autoState(286, 280, 5), "OP") == 0);
    CHECK(std::strcmp(Fmt::autoState(285, 280, 5), "HD") == 0);    // 경계는 그대로
    CHECK(std::strcmp(Fmt::autoState(275, 280, 5), "HD") == 0);
    CHECK(std::strcmp(Fmt::autoState(274, 280, 5), "CL") == 0);
    char b[4] = {};
    Fmt::waitSecs3(b, 8);    CHECK(std::memcmp(b, "  8", 3) == 0);
    Fmt::waitSecs3(b, 45);   CHECK(std::memcmp(b, " 45", 3) == 0);
    Fmt::waitSecs3(b, 600);  CHECK(std::memcmp(b, "600", 3) == 0);
    Fmt::waitSecs3(b, 1200); CHECK(std::memcmp(b, "999", 3) == 0);
}

int main() {
    RUN(test_zigzag);
    RUN(test_status_led);
    RUN(test_basic_config);
    RUN(test_settings_load);
    RUN(test_auto_state);
    RUN(test_format_values);
    RUN(test_format_small);
    RUN(test_format_big);
    RUN(test_error_log);
    RUN(test_sensor_filter);
    RUN(test_temp_control);
    RUN(test_trend_hold_after_our_move);
    RUN(test_trend_outside_change_waits_period);
    RUN(test_period_follows_response);
    RUN(test_dispatcher_boot_and_move);
    RUN(test_dispatcher_estop_and_relay);
    RUN(test_dispatcher_temp_lost);
    RUN(test_dispatcher_sync_and_modes);
    RUN(test_uisim_plant);
    return finish();
}
