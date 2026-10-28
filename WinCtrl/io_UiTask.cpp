#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "io_UiTask.h"
#include "io_Format.h"
#include "io_Sound.h"
#include "app_ControlTask.h"
#include "Settings.h"
#include "Store.h"
#include "dev_Lcd.h"
#include "dev_Encoder.h"
#include "dev_Buzzer.h"
#include "pins.h"

namespace {

// ------------------------------------------------------------
//  설정 메뉴 항목
// ------------------------------------------------------------
enum Fmt_ : uint8_t {
    F_TIME,      // 0.5초 단위 시간, 비어 있으면 ---.-s
    F_LIMIT,     // 시간, 비어 있으면 OFF
    F_AMPS,      // mA, 0.1A로 보임
    F_RATE,      // 0.1배 "1.3x"
    F_MAXRUN,    // 초, -1이면 AUTO
    F_COUNT,     // 횟수, 0이면 OFF
    F_SENS,      // AUTO/OFF + FOUND/NONE
    F_TEMP,      // 0.1도 "28.0C"
    F_SECS,      // 초 정수 "60s"
    F_TIMEOFF,   // 시간, 0이면 OFF
    F_MIN,       // 분 "5min"
    F_MINOFF,    // 분, 0이면 OFF
    F_OFFSET,    // 0.1도 부호 있음
    F_ONOFF
};

struct FieldDef {
    const char* name;
    int32_t     min, max, step;
    uint8_t     fmt;
};

enum WinField : uint8_t {
    W_UP, W_DOWN, W_OVERLAP, W_BACKOFF, W_LIMIT, W_IMAXUP, W_IMAXDN, W_OVL,
    W_MAXRUN, W_REHOME, W_HOMETIME, W_HOMESENS, W_COUNT
};
const FieldDef WIN_FIELDS[W_COUNT] = {
    { "UP TIME",   RANGE_TIME_MIN, RANGE_TIME_MAX, CS_STEP, F_TIME },
    { "DOWN TIME", RANGE_TIME_MIN, RANGE_TIME_MAX, CS_STEP, F_TIME },
    { "OVERLAP",   0, RANGE_OVERLAP_MAX, CS_STEP, F_TIME },
    { "BACKOFF",   0, RANGE_BACKOFF_MAX, CS_STEP, F_TIME },
    { "LIMIT",     0, RANGE_TIME_MAX * 2, CS_STEP, F_LIMIT },
    { "I MAX UP",  RANGE_IMAX_MIN, RANGE_IMAX_MAX, 100, F_AMPS },
    { "I MAX DN",  RANGE_IMAX_MIN, RANGE_IMAX_MAX, 100, F_AMPS },
    { "OVL RATE",  RANGE_OVL_MIN, RANGE_OVL_MAX, 1, F_RATE },
    { "MAX RUN",   RANGE_MAXRUN_MIN, RANGE_MAXRUN_MAX, 1, F_MAXRUN },
    { "REHOME",    0, RANGE_REHOME_MAX, 1, F_COUNT },
    { "HOME TIME", 0, RANGE_HOME_MAX, CS_STEP, F_TIME },
    { "HOME SENS", 0, 1, 1, F_SENS },
};

enum TempField : uint8_t {
    T_SENSOR, T_SET, T_DB, T_PERIOD, T_STEP, T_ADD, T_MAX, T_SLOPE, T_AHEAD, T_4, T_20, T_OFFSET, T_COUNT
};
const FieldDef TEMP_FIELDS[T_COUNT] = {
    { "T SENSOR",   0, 1, 1, F_SENS },
    { "T SET",      0, 500, 5, F_TEMP },
    { "T DEADBAND", 5, 50, 5, F_TEMP },
    { "T PERIOD",   10, 600, 10, F_SECS },
    { "T STEP",     50, 6000, CS_STEP, F_TIME },
    { "T STEP ADD", 0, 3000, CS_STEP, F_TIMEOFF },
    { "T STEP MAX", 50, 12000, CS_STEP, F_TIME },
    { "T SLOPE",    1, 30, 1, F_MIN },
    { "T AHEAD",    0, 15, 1, F_MINOFF },
    { "T AT 4MA",   -500, 500, 5, F_TEMP },
    { "T AT 20MA",  0, 1500, 5, F_TEMP },
    { "T OFFSET",   -50, 50, 1, F_OFFSET },
};

enum SoundField : uint8_t { S_CLICK, S_ALARM, S_COUNT };
const FieldDef SOUND_FIELDS[S_COUNT] = {
    { "CLICK SOUND", 0, 1, 1, F_ONOFF },
    { "ALARM SOUND", 0, 1, 1, F_ONOFF },
};

const char* const TOP_ITEMS[] = {
    "WINDOW 1", "WINDOW 2", "CALIBRATE", "TEMP CONTROL", "DIAG", "SOUND", "ERROR LOG", "INFO", "EXIT"
};
const uint8_t TOP_COUNT = 9;

enum class Screen : uint8_t { SPLASH, MAIN, MENU };

enum class Level : uint8_t {
    TOP, LIST, EDIT, CAL_SELECT, CAL_CONFIRM, CAL_RUN, CAL_RESULT, CAL_SAVE, DIAG, LOG, INFO, MSG
};

enum class ListKind : uint8_t { WIN, TEMP, SOUND };

const uint32_t LCD_RETRY_MS = 5000;
const uint32_t MSG_MS = 2000;

// ------------------------------------------------------------
class Ui {
public:
    void begin(Model model, const Settings& s, const ErrorLog& log, uint16_t bootCount,
               const char* date, const char* time);
    void tick();

private:
    // 입력
    void readInput(uint32_t now);
    void onShort(uint32_t now);
    void onLong(uint32_t now);
    void onSteps(const EncStep* st, uint8_t n, uint32_t now);
    int32_t stepSize(int32_t base, uint32_t intervalMs) const;

    // 평소 화면
    void mainShort(uint32_t now);
    void mainSteps(const int8_t* dirs, const uint32_t* gaps, uint8_t n, uint32_t now);
    void mainTick(uint32_t now);
    void drawMain(uint32_t now);
    bool tempMode() const { return _st.temp.found; }
    bool anyMoving() const;
    bool anyFault() const;
    void logLcd(const char* what) const;
    void startEdit(uint8_t w);
    void ackOne();

    // 설정 메뉴
    void enterMenu(uint32_t now);
    void exitMenu();
    void menuShort(uint32_t now);
    void menuSteps(const int8_t* dirs, const uint32_t* gaps, uint8_t n, uint32_t now);
    void menuTick(uint32_t now);
    void drawMenu(uint32_t now);
    uint8_t listCount() const;
    const FieldDef& field(uint8_t i) const;
    int32_t getField(const Settings& s, uint8_t i) const;
    void    setField(Settings& s, uint8_t i, int32_t v) const;
    void    fmtField(char* out, uint8_t i, int32_t v) const;
    int32_t stepField(uint8_t i, int32_t v, int8_t dir, int32_t mul, bool& bumped) const;
    void    saveField(uint32_t now);
    void    message(const char* l0, const char* l1, uint32_t now, bool waitPress, Level next);
    void    header(char* out, const char* left, uint16_t idx, uint16_t total) const;
    bool    sensorActive(uint8_t w, const Settings& s) const;
    void    saveCalibration(uint32_t now);
    void    pushSettings(uint8_t timesMask, uint8_t calMask);

    // 공통
    void consumeEvents(uint32_t now);
    void drawEstop(uint32_t now);
    void drawSplash();

    Lcd      _lcd;
    Encoder  _enc;
    Buzzer   _bz;
    Sound    _snd;

    Model    _model = Model::W300;
    Settings _s = {};
    ErrorLog _log;
    uint16_t _boot = 0;
    char     _dateLine[17] = {};
    char     _time[9] = {};
    Status   _st = {};
    WinState _prevState[2] = { WinState::IDLE, WinState::IDLE };
    uint32_t _seenEvents = 0;
    bool     _logDirty = false;

    Screen   _screen = Screen::SPLASH;
    uint32_t _startMs = 0;
    bool     _bootSoundDone = false;

    // 입력 상태
    bool     _btnDown = false;
    uint32_t _btnSince = 0;
    bool     _longFired = false;
    bool     _wakeOnly = false;
    uint32_t _lastInputMs = 0;
    bool     _backlight = true;
    uint8_t  _blKeepWhy = 0xFF;   // 시리얼 확인용: 켜 두는 이유가 바뀔 때만 찍는다
    int8_t   _pendDir = 0;
    uint32_t _pendMs = 0;
    bool     _burst = false;
    uint32_t _lastLcdInit = 0;

    // 평소 화면
    uint8_t  _sel = 0;            // 온도 없음: 0, 1 = 모터. 온도 있음: 0 = 설정 온도, 1, 2 = 모터
    uint32_t _selMs = 0;
    bool     _editing = false;
    uint8_t  _editWin = 0;
    cs_t     _editTarget = 0;
    bool     _editTemp = false;
    int16_t  _editSetT = 0;
    uint32_t _editMs = 0;
    bool     _jobPending = false;
    Job      _pendingJob = {};
    bool     _settingsPending = false;
    uint8_t  _pendTimes = 0, _pendCal = 0;

    // 설정 메뉴
    Level    _lv = Level::TOP;
    uint8_t  _top = 0;
    ListKind _list = ListKind::WIN;
    uint8_t  _listWin = 0;
    uint8_t  _idx = 0;
    int32_t  _val = 0;
    uint8_t  _page = 0;
    uint8_t  _calWin = 0;
    bool     _calSeen = false;
    uint32_t _calStartMs = 0;
    CalStatus _calRes = {};
    bool     _calYes = true;
    char     _msg0[17] = {}, _msg1[17] = {};
    uint32_t _msgUntil = 0;
    bool     _msgWait = false;
    Level    _msgNext = Level::TOP;
};

Ui s_ui;

void uiLoop(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        s_ui.tick();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(UI_PERIOD_MS));
    }
}

// ============================================================
void Ui::begin(Model model, const Settings& s, const ErrorLog& log, uint16_t bootCount,
               const char* date, const char* time) {
    _model = model;
    _s = s;
    _log = log;
    _boot = bootCount;
    Fmt::buildDate(_dateLine, date, time);
    strncpy(_time, time, 8);
    _time[8] = 0;

    _bz.begin(PIN_BUZZER);
    _snd.begin(&_bz);
    _snd.setOptions(s.clickSound, s.alarmSound);
    _enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);
    _lcd.begin(PIN_LCD_SDA, PIN_LCD_SCL);
    logLcd("begin");
    _startMs = millis();
    _lastInputMs = _startMs;
    _lastLcdInit = _startMs;
    _screen = Screen::SPLASH;
    drawSplash();
    _lcd.flush();

    // 전원 켤 때: 미확인 고장은 고장 상태로, 원점 센서 에러 기록은 센서 판단에 쓴다 (스펙 11절)
    bool history[2] = { false, false };
    bool restored[2] = { false, false };
    for (uint8_t i = 0; i < _log.count(); i++) {
        const ErrorEntry& e = _log.at(i);
        if (e.window < 1 || e.window > 2) continue;
        uint8_t w = (uint8_t)(e.window - 1);
        if (e.cause == Cause::HOME_SENSOR) history[w] = true;
        if (!e.acked && isFaultCause(e.cause) && e.cause != Cause::RELAY && !restored[w]) {
            Alert a = { AlertType::RESTORE_FAULT, w, e.cause };
            ControlTask::sendAlert(a);
            restored[w] = true;
        }
    }
    for (uint8_t w = 0; w < 2; w++) {
        if (history[w]) {
            Alert a = { AlertType::SENSOR_HISTORY, w, Cause::HOME_SENSOR };
            ControlTask::sendAlert(a);
        }
    }
    ControlTask::getStatus(_st);
    _seenEvents = _st.eventCount;
    xTaskCreatePinnedToCore(uiLoop, "ui", 8192, nullptr, 2, nullptr, 0);
}

bool Ui::anyMoving() const {
    return isMovingState(_st.win[0].state) || isMovingState(_st.win[1].state);
}

bool Ui::anyFault() const {
    return _st.win[0].state == WinState::FAULT || _st.win[1].state == WinState::FAULT || _st.relayFault;
}

// 시리얼 확인용: 화면을 찾았는지, 초기화가 됐는지
void Ui::logLcd(const char* what) const {
    if (_lcd.addr() == 0) {
        Serial.printf("[lcd] %s: no display\n", what);
        return;
    }
    Serial.printf("[lcd] %s 0x%02X %s %s, backlight %s\n", what, _lcd.addr(),
                  _lcd.kind(), _lcd.ok() ? "OK" : "FAIL", _backlight ? "ON" : "OFF");
}

bool Ui::sensorActive(uint8_t w, const Settings& s) const {
    // 아직 정해지지 않았으면 있는 것처럼 본다 (ctl_Window와 같은 규칙)
    return s.win[w].homeSensAuto && !_st.win[w].sensorAbsent && !_st.win[w].sensorError;
}

void Ui::pushSettings(uint8_t timesMask, uint8_t calMask) {
    _pendTimes |= timesMask;
    _pendCal |= calMask;
    _settingsPending = !ControlTask::sendSettings(_s, _pendTimes, _pendCal);
    if (!_settingsPending) { _pendTimes = 0; _pendCal = 0; }
}

// ============================================================
//  주기
// ============================================================
void Ui::tick() {
    uint32_t now = millis();
    ControlTask::getStatus(_st);
    consumeEvents(now);

    // 보내지 못한 잡과 설정은 다시 보낸다
    if (_jobPending && ControlTask::sendJob(_pendingJob)) _jobPending = false;
    if (_settingsPending) pushSettings(0, 0);

    readInput(now);

    if (_screen == Screen::SPLASH) {
        // 자가 점검에서 문제가 나오면 기다리지 않고 넘어간다
        if (now - _startMs >= SPLASH_MS || _st.relayFault) _screen = Screen::MAIN;
    }
    if (!_bootSoundDone && _st.bootDone) {
        _bootSoundDone = true;
        if (!_st.relayFault) _snd.play(Snd::BOOT_OK);
    }

    // 상한 리밋에서 정지
    for (uint8_t i = 0; i < 2; i++) {
        if (_st.win[i].state == WinState::LIMIT && _prevState[i] != WinState::LIMIT) _snd.play(Snd::LIMIT_STOP);
        _prevState[i] = _st.win[i].state;
    }

    if (_screen == Screen::MAIN) mainTick(now);
    else if (_screen == Screen::MENU) menuTick(now);

    if (_screen == Screen::SPLASH) drawSplash();
    else if (_st.estop) drawEstop(now);
    else if (_screen == Screen::MAIN) drawMain(now);
    else drawMenu(now);

    // 백라이트: 움직이는 동안, 고장, 비상정지에서는 켜 둔다
    uint8_t why = (anyMoving() ? 1 : 0) | (anyFault() ? 2 : 0) | (_st.estop ? 4 : 0) |
                  (_screen == Screen::SPLASH ? 8 : 0);
    if (why != _blKeepWhy) {
        _blKeepWhy = why;
        Serial.printf("[lcd] backlight held by:%s%s%s%s%s\n", why ? "" : " none (off after idle)",
                      (why & 1) ? " moving" : "", (why & 2) ? " fault" : "",
                      (why & 4) ? " estop" : "", (why & 8) ? " splash" : "");
    }
    bool keep = why != 0;
    bool bl = keep || now - _lastInputMs < BACKLIGHT_OFF_MS;
    if (bl != _backlight) Serial.printf("[lcd] backlight %s\n", bl ? "ON" : "OFF");
    _backlight = bl;
    _lcd.setBacklight(_backlight);

    // 화면 통신: 주기적으로, 실패했으면 더 자주 다시 초기화
    uint32_t every = _lcd.ok() ? LCD_REINIT_MS : LCD_RETRY_MS;
    if (now - _lastLcdInit >= every) { _lastLcdInit = now; _lcd.reinit(); logLcd("reinit"); }
    _lcd.flush();

    _snd.setOptions(_s.clickSound, _s.alarmSound);
    _snd.tick(now, anyFault(), _st.estop);

    if (_logDirty) { _logDirty = false; Store::saveErrors(_log); }
}

void Ui::consumeEvents(uint32_t now) {
    uint32_t count = _st.eventCount;
    if (count == _seenEvents) return;
    uint32_t from = _seenEvents;
    if (count - from > EVENT_RING_SIZE) from = count - EVENT_RING_SIZE;   // 놓친 것은 버림
    for (uint32_t k = from; k < count; k++) {
        const ErrorEvent& e = _st.events[k % EVENT_RING_SIZE];
        if (_log.add(e.window, e.cause, _boot, now / 1000)) _logDirty = true;
        if (e.cause == Cause::HOME_SENSOR || e.cause == Cause::TEMP_LOST || e.cause == Cause::TEMP_BAD)
            _snd.play(Snd::ALERT);
    }
    _seenEvents = count;
}

// ============================================================
//  입력
// ============================================================
int32_t Ui::stepSize(int32_t base, uint32_t gap) const {
    // 빨리 돌리면 5초, 10초 단위로 (0.5초 단위 값). 다른 값은 5배, 10배
    if (base == CS_STEP) {
        if (gap < ENC_FASTER_MS) return 1000;
        if (gap < ENC_FAST_MS) return 500;
        return base;
    }
    if (gap < ENC_FASTER_MS) return base * 10;
    if (gap < ENC_FAST_MS) return base * 5;
    return base;
}

void Ui::readInput(uint32_t now) {
    EncStep steps[16];
    uint8_t n = _enc.take(steps, 16);
    bool down = _enc.pressed();

    // 시리얼 확인용: 엔코더 한 칸과 누름/뗌을 찍는다
    for (uint8_t i = 0; i < n; i++)
        Serial.printf("[enc] step %s (edges %lu)\n", steps[i].dir > 0 ? "CW +1" : "CCW -1",
                      (unsigned long)_enc.edgeCount());
    if (down != _btnDown) Serial.printf("[enc] button %s\n", down ? "DOWN" : "UP");

    if (n || down != _btnDown) {
        bool wasDark = !_backlight;
        _lastInputMs = now;
        if (wasDark) {
            // 꺼진 화면의 첫 조작은 화면만 켠다
            Serial.println("[lcd] backlight ON (input)");
            _backlight = true;
            _lcd.setBacklight(true);
            n = 0;
            if (down && !_btnDown) _wakeOnly = true;
        }
    }

    if (down && !_btnDown) { _btnSince = now; _longFired = false; }
    if (down && !_longFired && !_wakeOnly && now - _btnSince >= LONG_PRESS_MS) {
        _longFired = true;
        onLong(now);
    }
    if (!down && _btnDown) {
        if (!_longFired && !_wakeOnly) onShort(now);
        _wakeOnly = false;
    }
    _btnDown = down;

    if (n) onSteps(steps, n, now);
}

void Ui::onShort(uint32_t now) {
    if (_screen == Screen::SPLASH || _st.estop) return;
    if (_screen == Screen::MAIN) mainShort(now);
    else menuShort(now);
}

void Ui::onLong(uint32_t now) {
    if (_screen == Screen::SPLASH || _st.estop) return;
    if (_screen == Screen::MAIN) {
        if (anyMoving() || !_st.bootDone) { _snd.play(Snd::ERROR_BEEP); return; }
        _snd.play(Snd::LONG_PRESS);
        enterMenu(now);
    } else {
        // 메뉴에서 길게 누르면 취소하고 나간다. 측정 중이면 멈춘다
        _snd.play(Snd::LONG_PRESS);
        if (_lv == Level::CAL_RUN) {
            Alert a = { AlertType::STOP_ALL, 0xFF, Cause::NONE };
            ControlTask::sendAlert(a);
        }
        exitMenu();
    }
}

void Ui::onSteps(const EncStep* st, uint8_t n, uint32_t now) {
    if (_screen == Screen::SPLASH || _st.estop) return;
    int8_t dirs[16];
    uint32_t gaps[16];
    uint8_t m = 0;
    bool strict = _screen == Screen::MAIN;
    for (uint8_t i = 0; i < n; i++) {
        uint32_t gap = st[i].ms - _pendMs;
        bool same = _pendDir == st[i].dir && gap <= ENC_VALID_WINDOW_MS;
        if (!strict) {
            dirs[m] = st[i].dir; gaps[m] = same ? gap : 1000; m++;
        } else if (same) {
            // 0.5초 안에 같은 방향 두 칸 이상이어야 입력으로 받는다. 첫 칸도 함께 넣는다
            if (!_burst) { dirs[m] = st[i].dir; gaps[m] = 1000; m++; _burst = true; }
            dirs[m] = st[i].dir; gaps[m] = gap; m++;
        } else {
            _burst = false;
        }
        _pendDir = st[i].dir;
        _pendMs = st[i].ms;
        if (m >= 15) break;
    }
    if (!m) return;
    if (_screen == Screen::MAIN) mainSteps(dirs, gaps, m, now);
    else menuSteps(dirs, gaps, m, now);
}

// ============================================================
//  평소 화면
// ============================================================
void Ui::ackOne() {
    ErrorEntry e;
    if (_log.ackOldest(e)) {
        _logDirty = true;
        if (e.window >= 1 && e.window <= 2) {
            uint8_t w = (uint8_t)(e.window - 1);
            if (isFaultCause(e.cause)) {
                Alert a = { AlertType::CLEAR_FAULT, w, e.cause };
                ControlTask::sendAlert(a);
            } else if (e.cause == Cause::HOME_SENSOR) {
                Alert a = { AlertType::RECHECK_SENSOR, w, e.cause };
                ControlTask::sendAlert(a);
            }
        } else if (isFaultCause(e.cause)) {
            Alert a = { AlertType::CLEAR_FAULT, 0xFF, e.cause };
            ControlTask::sendAlert(a);
        }
        return;
    }
    // 기록 없이 고장으로 선 창 (기록이 밀려난 경우)
    for (uint8_t w = 0; w < 2; w++) {
        if (_st.win[w].state == WinState::FAULT) {
            Alert a = { AlertType::CLEAR_FAULT, w, _st.win[w].fault };
            ControlTask::sendAlert(a);
        }
    }
}

void Ui::mainShort(uint32_t now) {
    // 순서: 움직이면 정지, 다음이 에러 확인, 그다음 입력 취소나 선택 이동
    if (anyMoving()) {
        Alert a = { AlertType::STOP_ALL, 0xFF, Cause::NONE };
        ControlTask::sendAlert(a);
        _editing = false;
        _editTemp = false;
        _jobPending = false;
        if (tempMode()) _sel = 0;
        return;
    }
    if (_log.hasUnacked() || _st.win[0].state == WinState::FAULT || _st.win[1].state == WinState::FAULT) {
        ackOne();
        return;
    }
    if (_editing || _editTemp) {
        _editing = false;
        _editTemp = false;
        if (tempMode()) _sel = 0;
        return;
    }
    if (tempMode()) _sel = (uint8_t)((_sel + 1) % 3);
    else            _sel = (uint8_t)((_sel + 1) % 2);
    _selMs = now;
}

void Ui::startEdit(uint8_t w) {
    const WindowStatus& ws = _st.win[w];
    cs_t base = 0;
    if (isMovingState(ws.state) && ws.state != WinState::CALIBRATING) base = ws.target;   // -1 = CLOSE
    else if (ws.posState == PosState::CLOSED) base = -1;
    else if (ws.posState == PosState::VALID) base = (ws.pos + CS_STEP / 2) / CS_STEP * CS_STEP;
    if (base > 0 && tempMode()) base = (base + 50) / 100 * 100;            // 화면에 보이는 정수 초에서 시작
    _editing = true;
    _editWin = w;
    _editTarget = base;
}

void Ui::mainSteps(const int8_t* dirs, const uint32_t* gaps, uint8_t n, uint32_t now) {
    if (!_st.bootDone) return;
    bool changed = false, bumped = false;

    if (tempMode() && _sel == 0) {
        // 설정 온도: 0.5도씩
        if (!_editTemp) { _editTemp = true; _editSetT = _s.temp.setT; }
        for (uint8_t i = 0; i < n; i++) {
            int32_t v = _editSetT + dirs[i] * 5;
            if (v < 0 || v > 500) { bumped = true; continue; }
            _editSetT = (int16_t)v;
            changed = true;
        }
    } else {
        uint8_t w = tempMode() ? (uint8_t)(_sel - 1) : _sel;
        const WindowStatus& ws = _st.win[w];
        if (ws.state == WinState::FAULT || ws.state == WinState::ESTOP || ws.state == WinState::CALIBRATING
            || !ws.configured) {
            _snd.play(Snd::ERROR_BEEP);
            return;
        }
        if (!_editing || _editWin != w) startEdit(w);
        cs_t top = ws.limit / CS_STEP * CS_STEP;
        for (uint8_t i = 0; i < n; i++) {
            cs_t step = stepSize(tempMode() ? 100 : CS_STEP, gaps[i]);
            cs_t t = _editTarget;
            if (dirs[i] > 0) {
                if (t < 0) t = 0;                         // CLOSE에서 한 칸 위는 0.0초
                else if (t >= top) { bumped = true; continue; }
                else { t += step; if (t > top) t = top; }
            } else {
                if (t < 0) { bumped = true; continue; }
                else if (t == 0) t = -1;                  // 0.0초 아래로 한 칸 더 돌리면 CLOSE
                else { t -= step; if (t < 0) t = 0; }
            }
            _editTarget = t;
            changed = true;
        }
    }
    _editMs = now;
    _selMs = now;
    if (changed) _snd.play(Snd::CLICK);          // 한 주기에 여러 칸이어도 한 번
    else if (bumped) _snd.play(Snd::BUMP);
}

void Ui::mainTick(uint32_t now) {
    // 입력 확정: 2초 동안 변경 없음
    if (_editTemp && now - _editMs >= INPUT_COMMIT_MS) {
        _editTemp = false;
        if (_editSetT != _s.temp.setT) {
            _s.temp.setT = _editSetT;
            Store::saveSettings(_s);
            pushSettings(0, 0);
            _snd.play(Snd::COMMIT);
        }
    }
    if (_editing && now - _editMs >= INPUT_COMMIT_MS) {
        _editing = false;
        Job j = {};
        j.source = JobSource::ENCODER;
        j.window = _editWin;
        if (_editTarget < 0) j.type = JobType::CLOSE;
        else { j.type = JobType::MOVE; j.target = _editTarget; }
        _pendingJob = j;
        _jobPending = !ControlTask::sendJob(j);
        _snd.play(Snd::COMMIT);
        if (tempMode()) _sel = 0;                 // 모터 입력이 끝나면 설정 온도로
    }
    // 온도제어 화면: 모터를 고르기만 하고 2초 동안 조작이 없으면 설정 온도로
    if (tempMode()) {
        if (_sel > 2) _sel = 0;
        if (_sel != 0 && !_editing && now - _selMs >= INPUT_COMMIT_MS) _sel = 0;
    } else {
        if (_sel > 1) _sel = 0;
        _editTemp = false;
    }
}

void Ui::drawMain(uint32_t now) {
    bool blinkOn = (now / BLINK_MS) % 2 == 0;
    bool alt = (now / ALT_TEXT_MS) % 2 == 1;
    char line[17];
    for (uint8_t i = 0; i < 2; i++) {
        Fmt::LineCtx c;
        c.num = (uint8_t)(i + 1);
        c.editing = _editing && _editWin == i;
        c.editTarget = _editTarget;
        c.blinkOn = blinkOn;
        c.altPhase = alt;
        c.sensorErr = _log.hasUnacked((uint8_t)(i + 1), Cause::HOME_SENSOR);
        uint8_t selIdx = tempMode() ? (uint8_t)(i + 1) : i;
        c.sel = c.editing ? '*' : (_sel == selIdx ? '>' : ' ');
        if (!tempMode()) {
            Fmt::winLine(line, _st.win[i], c);
        } else {
            char right[9];
            Fmt::winShort(right, _st.win[i], c);
            if (i == 0) {
                Fmt::tempLine(line, ' ', _st.temp.valid, _st.temp.temp, 'C', right);
            } else {
                char sel = _editTemp ? '*' : (_sel == 0 ? '>' : ' ');
                Fmt::tempLine(line, sel, true, _editTemp ? _editSetT : _s.temp.setT, 'S', right);
            }
        }
        _lcd.setLine(i, line);
    }
}

void Ui::drawEstop(uint32_t now) {
    bool on = (now / BLINK_MS) % 2 == 0;
    _lcd.setLine(0, on ? " *** E-STOP *** " : "");
    _lcd.setLine(1, " RELEASE BUTTON ");
}

void Ui::drawSplash() {
    char top[17];
    Fmt::splashTop(top, PRODUCT_NAME, FW_VERSION_MAJOR, FW_VERSION_MINOR);
    _lcd.setLine(0, top);
    _lcd.setLine(1, _dateLine);
}

// ============================================================
//  설정 메뉴
// ============================================================
void Ui::enterMenu(uint32_t now) {
    _editing = false;
    _editTemp = false;
    _screen = Screen::MENU;
    _lv = Level::TOP;
    _top = 0;
    _lastInputMs = now;
    Alert a = { AlertType::MENU_ENTER, 0xFF, Cause::NONE };
    ControlTask::sendAlert(a);
}

void Ui::exitMenu() {
    _screen = Screen::MAIN;
    _lv = Level::TOP;
    _sel = 0;
    Alert a = { AlertType::MENU_EXIT, 0xFF, Cause::NONE };
    ControlTask::sendAlert(a);
}

void Ui::message(const char* l0, const char* l1, uint32_t now, bool waitPress, Level next) {
    strncpy(_msg0, l0, 16); _msg0[16] = 0;
    strncpy(_msg1, l1, 16); _msg1[16] = 0;
    _msgWait = waitPress;
    _msgUntil = now + MSG_MS;
    _msgNext = next;
    _lv = Level::MSG;
}

void Ui::header(char* out, const char* left, uint16_t idx, uint16_t total) const {
    char cnt[8];
    snprintf(cnt, sizeof(cnt), "%u/%u", (unsigned)idx, (unsigned)total);
    memset(out, ' ', 16);
    out[16] = 0;
    size_t n = strlen(left);
    memcpy(out, left, n > 16 ? 16 : n);
    size_t m = strlen(cnt);
    memcpy(out + 16 - m, cnt, m);
}

uint8_t Ui::listCount() const {
    switch (_list) {
    case ListKind::WIN:  return W_COUNT;
    case ListKind::TEMP: return T_COUNT;
    default:             return S_COUNT;
    }
}

const FieldDef& Ui::field(uint8_t i) const {
    switch (_list) {
    case ListKind::WIN:  return WIN_FIELDS[i];
    case ListKind::TEMP: return TEMP_FIELDS[i];
    default:             return SOUND_FIELDS[i];
    }
}

int32_t Ui::getField(const Settings& s, uint8_t i) const {
    if (_list == ListKind::WIN) {
        const WindowSettings& w = s.win[_listWin];
        switch (i) {
        case W_UP: return w.upTime;
        case W_DOWN: return w.downTime;
        case W_OVERLAP: return w.overlap;
        case W_BACKOFF: return w.backoff;
        case W_LIMIT: return w.limit;
        case W_IMAXUP: return w.iMaxUp;
        case W_IMAXDN: return w.iMaxDn;
        case W_OVL: return w.ovlRate;
        case W_MAXRUN: return w.maxRunS;
        case W_REHOME: return w.rehome;
        case W_HOMETIME: return w.homeTime;
        default: return w.homeSensAuto ? 1 : 0;
        }
    }
    if (_list == ListKind::TEMP) {
        const TempSettings& t = s.temp;
        switch (i) {
        case T_SENSOR: return t.sensorAuto ? 1 : 0;
        case T_SET: return t.setT;
        case T_DB: return t.deadband;
        case T_PERIOD: return t.periodS;
        case T_STEP: return t.step;
        case T_ADD: return t.stepAdd;
        case T_MAX: return t.stepMax;
        case T_SLOPE: return t.slopeMin;
        case T_AHEAD: return t.aheadMin;
        case T_4: return t.t4;
        case T_20: return t.t20;
        default: return t.offset;
        }
    }
    return i == S_CLICK ? (s.clickSound ? 1 : 0) : (s.alarmSound ? 1 : 0);
}

void Ui::setField(Settings& s, uint8_t i, int32_t v) const {
    if (_list == ListKind::WIN) {
        WindowSettings& w = s.win[_listWin];
        switch (i) {
        case W_UP: w.upTime = v; break;
        case W_DOWN: w.downTime = v; break;
        case W_OVERLAP: w.overlap = v; break;
        case W_BACKOFF: w.backoff = v; break;
        case W_LIMIT: w.limit = v; break;
        case W_IMAXUP: w.iMaxUp = v; break;
        case W_IMAXDN: w.iMaxDn = v; break;
        case W_OVL: w.ovlRate = (uint8_t)v; break;
        case W_MAXRUN: w.maxRunS = (int16_t)v; break;
        case W_REHOME: w.rehome = (uint8_t)v; break;
        case W_HOMETIME: w.homeTime = v; break;
        default: w.homeSensAuto = v != 0; break;
        }
        return;
    }
    if (_list == ListKind::TEMP) {
        TempSettings& t = s.temp;
        switch (i) {
        case T_SENSOR: t.sensorAuto = v != 0; break;
        case T_SET: t.setT = (int16_t)v; break;
        case T_DB: t.deadband = (int16_t)v; break;
        case T_PERIOD: t.periodS = (uint16_t)v; break;
        case T_STEP: t.step = v; break;
        case T_ADD: t.stepAdd = v; break;
        case T_MAX: t.stepMax = v; break;
        case T_SLOPE: t.slopeMin = (uint8_t)v; break;
        case T_AHEAD: t.aheadMin = (uint8_t)v; break;
        case T_4: t.t4 = (int16_t)v; break;
        case T_20: t.t20 = (int16_t)v; break;
        default: t.offset = (int16_t)v; break;
        }
        return;
    }
    if (i == S_CLICK) s.clickSound = v != 0;
    else s.alarmSound = v != 0;
}

void Ui::fmtField(char* out, uint8_t i, int32_t v) const {
    const FieldDef& f = field(i);
    char b[16];
    switch (f.fmt) {
    case F_TIME:
        Fmt::secs(b, v, 5); snprintf(out, 16, "%ss", b); break;
    case F_LIMIT:
        if (v < 0) strcpy(out, "OFF");
        else { Fmt::secs(b, v, 5); snprintf(out, 16, "%ss", b); }
        break;
    case F_AMPS:
        Fmt::amps(b, v); snprintf(out, 16, "%s", v < 0 ? "--.-A" : b); break;
    case F_RATE:
        snprintf(out, 16, "%ld.%ldx", (long)(v / 10), (long)(v % 10)); break;
    case F_MAXRUN:
        if (v < 0) strcpy(out, "AUTO");
        else snprintf(out, 16, "%lds", (long)v);
        break;
    case F_COUNT:
        if (v == 0) strcpy(out, "OFF");
        else snprintf(out, 16, "%ld", (long)v);
        break;
    case F_SENS: {
        const char* r;
        if (_list == ListKind::TEMP) r = _st.temp.found ? "FOUND" : "NONE";
        else {
            const WindowStatus& w = _st.win[_listWin];
            r = w.sensorFound ? "FOUND" : (w.sensorAbsent ? "NONE" : "WAIT");   // WAIT: ON이나 하한을 아직 못 봄
        }
        snprintf(out, 16, "%-6s %s", v ? "AUTO" : "OFF", r);
        break;
    }
    case F_TEMP: {
        long a = v < 0 ? -v : v;
        snprintf(out, 16, "%s%ld.%ldC", v < 0 ? "-" : "", a / 10, a % 10);
        break;
    }
    case F_SECS:
        snprintf(out, 16, "%lds", (long)v); break;
    case F_TIMEOFF:
        if (v == 0) strcpy(out, "OFF");
        else { Fmt::secs(b, v, 5); snprintf(out, 16, "%ss", b); }
        break;
    case F_MIN:
        snprintf(out, 16, "%ldmin", (long)v); break;
    case F_MINOFF:
        if (v == 0) strcpy(out, "OFF");
        else snprintf(out, 16, "%ldmin", (long)v);
        break;
    case F_OFFSET:
        Fmt::tenthsSigned(b, v); snprintf(out, 16, "%sC", b); break;
    default:
        strcpy(out, v ? "ON" : "OFF"); break;
    }
}

int32_t Ui::stepField(uint8_t i, int32_t v, int8_t dir, int32_t mul, bool& bumped) const {
    const FieldDef& f = field(i);
    if (f.fmt == F_SENS || f.fmt == F_ONOFF) return v ? 0 : 1;

    if (f.fmt == F_LIMIT) {
        // OFF는 최대 위치 위쪽 끝에 둔다
        Settings tmp = _s;
        WindowSettings& w = tmp.win[_listWin];
        cs_t top = SettingsRules::maxPos(w, sensorActive(_listWin, tmp)) / CS_STEP * CS_STEP;
        if (w.upTime < 0) { bumped = v < 0; return CS_EMPTY; }
        if (v < 0) {
            if (dir > 0) { bumped = true; return v; }
            return top;
        }
        int32_t n = v + dir * mul;
        if (dir > 0 && n > top) return CS_EMPTY;
        if (n < 0) { if (v == 0) bumped = true; n = 0; }
        return n;
    }
    if (f.fmt == F_MAXRUN) {
        if (v < 0) { if (dir < 0) { bumped = true; return v; } return f.min; }
        int32_t n = v + dir * mul;
        if (n < f.min) return -1;                  // 아래 끝은 AUTO
        if (n > f.max) { bumped = v == f.max; n = f.max; }
        return n;
    }
    if (v < f.min || v > f.max) {                  // 비어 있던 값은 아래 끝에서 시작
        return f.min;
    }
    int32_t n = v + dir * mul;
    if (n < f.min) { bumped = v == f.min; n = f.min; }
    if (n > f.max) { bumped = v == f.max; n = f.max; }
    return n;
}

void Ui::saveField(uint32_t now) {
    Settings tmp = _s;
    setField(tmp, _idx, _val);
    uint8_t times = 0;
    if (_list == ListKind::WIN) {
        uint8_t w = _listWin;
        bool active = sensorActive(w, tmp);
        bool timeField = _idx == W_UP || _idx == W_DOWN || _idx == W_OVERLAP || _idx == W_BACKOFF
                      || _idx == W_HOMETIME || _idx == W_HOMESENS;
        // 센서가 있는 창은 되내림이 바뀌면 원점 시간을 다시 계산해 둔다 (스펙 10절)
        if (active && (_idx == W_BACKOFF || _idx == W_UP || _idx == W_HOMESENS)) SettingsRules::recomputeHomeTime(tmp.win[w]);
        if (!SettingsRules::validWindow(tmp.win[w], active)) {
            _snd.play(Snd::ERROR_BEEP);
            message("NOT SAVED", "OUT OF RANGE", now, false, Level::EDIT);
            return;
        }
        if (timeField && getField(_s, _idx) != _val) times = (uint8_t)(1 << w);
    } else if (_list == ListKind::TEMP) {
        if (!SettingsRules::validTemp(tmp.temp)) {
            _snd.play(Snd::ERROR_BEEP);
            message("NOT SAVED", "OUT OF RANGE", now, false, Level::EDIT);
            return;
        }
    }
    _s = tmp;
    if (!Store::saveSettings(_s)) _snd.play(Snd::ERROR_BEEP);
    else _snd.play(Snd::COMMIT);
    pushSettings(times, 0);
    _lv = Level::LIST;
}

void Ui::saveCalibration(uint32_t now) {
    uint8_t w = _calWin;
    Settings tmp = _s;
    WindowSettings& ws = tmp.win[w];
    ws.upTime = _calRes.effUp;
    ws.downTime = _calRes.effDown;
    if (_calRes.sensorMode) ws.overlap = _calRes.overlap;
    ws.totalUp = _calRes.totalUp;
    ws.iMaxUp = _calRes.iMaxUp;
    ws.iMaxDn = _calRes.iMaxDn;
    if (_calRes.sensorMode) SettingsRules::recomputeHomeTime(ws);
    if (!SettingsRules::validWindow(ws, _calRes.sensorMode)) {
        _snd.play(Snd::ERROR_BEEP);
        message("NOT SAVED", "OUT OF RANGE", now, true, Level::TOP);
        return;
    }
    _s = tmp;
    Store::saveSettings(_s);
    pushSettings((uint8_t)(1 << w), (uint8_t)(1 << w));
    _snd.play(Snd::COMMIT);
    // 측정은 리밋을 풀어야 시작하므로 끝나면 다시 넣으라고 안내한다
    message("SAVED", "SET LIMIT AGAIN", now, true, Level::TOP);
}

void Ui::menuShort(uint32_t now) {
    switch (_lv) {
    case Level::TOP:
        switch (_top) {
        case 0: case 1:
            _list = ListKind::WIN; _listWin = _top; _idx = 0; _lv = Level::LIST; break;
        case 2: _idx = 0; _lv = Level::CAL_SELECT; break;
        case 3: _list = ListKind::TEMP; _idx = 0; _lv = Level::LIST; break;
        case 4: _page = 0; _lv = Level::DIAG; break;
        case 5: _list = ListKind::SOUND; _idx = 0; _lv = Level::LIST; break;
        case 6: _page = 0; _lv = Level::LOG; break;
        case 7: _page = 0; _lv = Level::INFO; break;
        default: exitMenu(); break;
        }
        break;
    case Level::LIST:
        if (_idx >= listCount()) { _lv = Level::TOP; break; }       // BACK
        _val = getField(_s, _idx);
        _lv = Level::EDIT;
        break;
    case Level::EDIT:
        saveField(now);
        break;
    case Level::CAL_SELECT: {
        if (_idx >= 2) { _lv = Level::TOP; break; }
        uint8_t w = _idx;
        _calWin = w;
        const WindowSettings& ws = _s.win[w];
        if (ws.limit != CS_EMPTY) { message("CALIBRATE", "LIMIT IS ON", now, false, Level::CAL_SELECT); break; }
        if (_st.win[w].state == WinState::FAULT) { message("CALIBRATE", "CLEAR FAULT", now, false, Level::CAL_SELECT); break; }
        if (!sensorActive(w, _s) && ws.homeTime < 0) { message("CALIBRATE", "SET HOME TIME", now, false, Level::CAL_SELECT); break; }
        _lv = Level::CAL_CONFIRM;
        break;
    }
    case Level::CAL_CONFIRM: {
        Job j = {};
        j.type = JobType::CALIBRATE;
        j.source = JobSource::ENCODER;
        j.window = _calWin;
        if (!ControlTask::sendJob(j)) { _snd.play(Snd::ERROR_BEEP); break; }
        _calSeen = false;
        _calStartMs = now;
        _lv = Level::CAL_RUN;
        break;
    }
    case Level::CAL_RUN: {
        Alert a = { AlertType::STOP_ALL, 0xFF, Cause::NONE };     // 짧게 누르면 측정 중단
        ControlTask::sendAlert(a);
        break;
    }
    case Level::CAL_RESULT:
        _calYes = true;
        _lv = Level::CAL_SAVE;
        break;
    case Level::CAL_SAVE:
        if (_calYes) saveCalibration(now);
        else message("NOT SAVED", "", now, false, Level::TOP);
        break;
    case Level::DIAG:
    case Level::LOG:
    case Level::INFO:
        _lv = Level::TOP;
        break;
    case Level::MSG:
        if (_msgWait) _lv = _msgNext;
        break;
    }
}

void Ui::menuSteps(const int8_t* dirs, const uint32_t* gaps, uint8_t n, uint32_t now) {
    (void)now;
    bool changed = false, bumped = false;
    for (uint8_t k = 0; k < n; k++) {
        int8_t d = dirs[k];
        switch (_lv) {
        case Level::TOP: {
            int t = _top + d;
            if (t < 0 || t >= TOP_COUNT) { bumped = true; break; }
            _top = (uint8_t)t; changed = true; break;
        }
        case Level::LIST: {
            int t = _idx + d;                      // 마지막 칸은 BACK
            if (t < 0 || t > listCount()) { bumped = true; break; }
            _idx = (uint8_t)t; changed = true; break;
        }
        case Level::EDIT: {
            const FieldDef& f = field(_idx);
            int32_t mul = stepSize(f.step, gaps[k]);
            bool b = false;
            int32_t v = stepField(_idx, _val, d, mul, b);
            if (b) bumped = true;
            if (v != _val) { _val = v; changed = true; }
            break;
        }
        case Level::CAL_SELECT: {
            int t = _idx + d;
            if (t < 0 || t > 2) { bumped = true; break; }
            _idx = (uint8_t)t; changed = true; break;
        }
        case Level::CAL_RESULT: {
            int t = _page + d;
            if (t < 0 || t > 6) { bumped = true; break; }
            _page = (uint8_t)t; changed = true; break;
        }
        case Level::CAL_SAVE:
            _calYes = !_calYes; changed = true; break;
        case Level::DIAG: {
            uint8_t pages = _st.temp.found ? 4 : 3;
            int t = _page + d;
            if (t < 0 || t >= pages) { bumped = true; break; }
            _page = (uint8_t)t; changed = true; break;
        }
        case Level::LOG: {
            int t = _page + d;
            if (t < 0 || t >= (_log.count() ? _log.count() : 1)) { bumped = true; break; }
            _page = (uint8_t)t; changed = true; break;
        }
        case Level::INFO: {
            int t = _page + d;
            if (t < 0 || t > 2) { bumped = true; break; }
            _page = (uint8_t)t; changed = true; break;
        }
        default:
            break;
        }
    }
    if (changed) _snd.play(Snd::CLICK);
    else if (bumped) _snd.play(Snd::BUMP);
}

void Ui::menuTick(uint32_t now) {
    if (_lv == Level::MSG && !_msgWait && (int32_t)(now - _msgUntil) >= 0) _lv = _msgNext;

    if (_lv == Level::CAL_RUN) {
        const WindowStatus& w = _st.win[_calWin];
        if (w.state == WinState::CALIBRATING) _calSeen = true;
        char l0[17];
        if (_calSeen && w.state != WinState::CALIBRATING) {
            _calRes = w.cal;
            if (_calRes.phase == CalPhase::DONE && !_calRes.outOfRange) {
                _page = 0;
                _lv = Level::CAL_RESULT;
            } else if (_calRes.phase == CalPhase::DONE) {
                snprintf(l0, sizeof(l0), "CAL W%u FAILED", (unsigned)(_calWin + 1));
                message(l0, "OUT OF RANGE", now, true, Level::TOP);
            } else if (_calRes.failCause == Cause::NONE) {
                snprintf(l0, sizeof(l0), "CAL W%u STOPPED", (unsigned)(_calWin + 1));
                message(l0, "", now, true, Level::TOP);
            } else {
                snprintf(l0, sizeof(l0), "CAL W%u FAILED", (unsigned)(_calWin + 1));
                message(l0, Fmt::faultLong(_calRes.failCause), now, true, Level::TOP);
            }
        } else if (!_calSeen && now - _calStartMs > 3000) {
            snprintf(l0, sizeof(l0), "CAL W%u FAILED", (unsigned)(_calWin + 1));
            message(l0, "NOT ACCEPTED", now, true, Level::TOP);
        }
        _lastInputMs = now;                        // 측정 중에는 메뉴 시간 제한을 두지 않는다
    }

    // 2분 동안 조작이 없으면 저장하지 않고 나간다
    if (now - _lastInputMs >= MENU_TIMEOUT_MS && !anyMoving()) exitMenu();
}

void Ui::drawMenu(uint32_t now) {
    char l0[17], l1[17], b[17];
    memset(l1, 0, sizeof(l1));
    bool blinkOn = (now / BLINK_MS) % 2 == 0;

    switch (_lv) {
    case Level::TOP:
        header(l0, "SETUP", (uint16_t)(_top + 1), TOP_COUNT);
        snprintf(l1, sizeof(l1), ">%s", TOP_ITEMS[_top]);
        break;
    case Level::LIST:
    case Level::EDIT: {
        char left[17];
        const char* prefix = _list == ListKind::WIN ? (_listWin == 0 ? "W1 " : "W2 ") : "";
        if (_idx >= listCount()) {
            const char* title = _list == ListKind::WIN ? (_listWin == 0 ? "WINDOW 1" : "WINDOW 2")
                              : (_list == ListKind::TEMP ? "TEMP CONTROL" : "SOUND");
            snprintf(l0, sizeof(l0), "%-16s", title);
            strcpy(l1, ">BACK");
            break;
        }
        snprintf(left, sizeof(left), "%s%s", prefix, field(_idx).name);
        header(l0, left, (uint16_t)(_idx + 1), listCount());
        fmtField(b, _idx, _lv == Level::EDIT ? _val : getField(_s, _idx));
        snprintf(l1, sizeof(l1), "%c%s", _lv == Level::EDIT ? '*' : ' ', b);
        break;
    }
    case Level::CAL_SELECT:
        snprintf(l0, sizeof(l0), "CALIBRATE");
        snprintf(l1, sizeof(l1), "%s", _idx == 0 ? ">WINDOW 1" : (_idx == 1 ? ">WINDOW 2" : ">BACK"));
        break;
    case Level::CAL_CONFIRM:
        snprintf(l0, sizeof(l0), "CAL W%u", (unsigned)(_calWin + 1));
        strcpy(l1, "PRESS TO START");
        break;
    case Level::CAL_RUN: {
        const WindowStatus& w = _st.win[_calWin];
        const char* ph = "START";
        switch (w.cal.phase) {
        case CalPhase::DOWN_FIRST: ph = "DOWN"; break;
        case CalPhase::WAIT_1: case CalPhase::WAIT_2: ph = "WAIT"; break;
        case CalPhase::UP: ph = "UP"; break;
        case CalPhase::DOWN: ph = "DOWN"; break;
        default: break;
        }
        snprintf(l0, sizeof(l0), "CAL W%u %-8s", (unsigned)(_calWin + 1), ph);
        l0[15] = w.dir > 0 ? (blinkOn ? '^' : ' ') : (w.dir < 0 ? (blinkOn ? 'v' : ' ') : ' ');
        l0[16] = 0;
        char t[8], a[10];
        Fmt::secs(t, w.cal.elapsed, 5);
        Fmt::amps(a, w.cal.currentMa);
        snprintf(l1, sizeof(l1), " %ss %7s", t, a);
        break;
    }
    case Level::CAL_RESULT: {
        static const char* const NAMES[7] = { "EFF UP", "EFF DOWN", "OVERLAP", "TOTAL UP", "I MAX", "I AVG", "SAVE" };
        char left[17];
        snprintf(left, sizeof(left), "W%u %s", (unsigned)(_calWin + 1), NAMES[_page]);
        header(l0, left, (uint16_t)(_page + 1), 7);
        char x[10], y[10];
        switch (_page) {
        case 0: Fmt::secs2(b, _calRes.effUp); snprintf(l1, sizeof(l1), " %ss", b); break;
        case 1: Fmt::secs2(b, _calRes.effDown); snprintf(l1, sizeof(l1), " %ss", b); break;
        case 2:
            if (_calRes.sensorMode) { Fmt::secs2(b, _calRes.overlap); snprintf(l1, sizeof(l1), " %ss", b); }
            else strcpy(l1, " NO SENSOR");
            break;
        case 3: Fmt::secs2(b, _calRes.totalUp); snprintf(l1, sizeof(l1), " %ss", b); break;
        case 4: Fmt::amps(x, _calRes.iMaxUp); Fmt::amps(y, _calRes.iMaxDn); snprintf(l1, sizeof(l1), "^%s v%s", x, y); break;
        case 5: Fmt::amps(x, _calRes.iAvgUp); Fmt::amps(y, _calRes.iAvgDn); snprintf(l1, sizeof(l1), "^%s v%s", x, y); break;
        default: strcpy(l1, " PRESS TO SAVE"); break;
        }
        break;
    }
    case Level::CAL_SAVE:
        snprintf(l0, sizeof(l0), "W%u SAVE RESULT?", (unsigned)(_calWin + 1));
        strcpy(l1, _calYes ? ">YES   NO" : " YES  >NO");
        break;
    case Level::DIAG: {
        uint8_t pages = _st.temp.found ? 4 : 3;
        if (_page == 0) {
            header(l0, "DIAG SENSOR", 1, pages);
            const char* s[2];
            for (uint8_t i = 0; i < 2; i++) {
                const WindowStatus& w = _st.win[i];
                if (!_s.win[i].homeSensAuto || w.sensorAbsent) s[i] = "--";
                else if (w.sensorError) s[i] = "ERR";
                else if (w.sensor == SensorState::PRESENT) s[i] = "ON";
                else if (w.sensor == SensorState::ABSENT) s[i] = "OFF";
                else s[i] = "--";
            }
            snprintf(l1, sizeof(l1), "S1 %-4s S2 %s", s[0], s[1]);
        } else if (_page <= 2) {
            uint8_t i = (uint8_t)(_page - 1);
            const WindowStatus& w = _st.win[i];
            char left[17];
            snprintf(left, sizeof(left), "DIAG W%u I", (unsigned)(i + 1));
            header(l0, left, _page + 1, pages);
            char m[10], u[10], d[10];
            Fmt::amps(m, w.moveMaxMa);
            Fmt::amps(u, w.overloadUpMa);
            Fmt::amps(d, w.overloadDnMa);
            snprintf(l1, sizeof(l1), "%s ^%s v%s", m, u, d);
        } else {
            header(l0, "DIAG TEMP", 4, pages);
            char t[8];
            Fmt::temp4(t, _st.temp.predicted);
            int32_t sl = _st.temp.slope;
            long a = sl < 0 ? -sl : sl;
            snprintf(l1, sizeof(l1), "P%sC %c%ld.%02ld/m", t, sl < 0 ? '-' : '+', a / 100, a % 100);
        }
        break;
    }
    case Level::LOG:
        if (_log.count() == 0) {
            strcpy(l0, "ERROR LOG");
            strcpy(l1, "NO ERRORS");
        } else {
            const ErrorEntry& e = _log.newest(_page);
            const char* w = e.window == 1 ? "W1" : (e.window == 2 ? "W2" : "ALL");
            snprintf(l0, sizeof(l0), "LOG %u/%u  %s", (unsigned)(_page + 1), (unsigned)_log.count(), w);
            snprintf(l1, sizeof(l1), "%-15s%c", Fmt::causeLog(e.cause), e.acked ? ' ' : '*');
        }
        break;
    case Level::INFO:
        if (_page == 0) {
            Fmt::splashTop(l0, PRODUCT_NAME, FW_VERSION_MAJOR, FW_VERSION_MINOR);
            strcpy(l1, _dateLine);
        } else if (_page == 1) {
            header(l0, "BUILD TIME", 2, 3);
            snprintf(l1, sizeof(l1), "%s", _time);
        } else {
            header(l0, "INFO", 3, 3);
            strcpy(l1, _model == Model::W50 ? "MODEL 50W" : "MODEL 300W");
        }
        break;
    case Level::MSG:
        strcpy(l0, _msg0);
        strcpy(l1, _msg1);
        break;
    }
    _lcd.setLine(0, l0);
    _lcd.setLine(1, l1);
}

}  // namespace

namespace UiTask {

void begin(Model model, const Settings& s, const ErrorLog& log, uint16_t bootCount,
           const char* buildDate, const char* buildTime) {
    s_ui.begin(model, s, log, bootCount, buildDate, buildTime);
}

}  // namespace UiTask
