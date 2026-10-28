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
    F_PCT,       // 위치 백분율, 99.9면 OFF (상한 리밋)
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
    W_MAXRUN, W_REHOME, W_HOMETIME, W_HOMESENS, W_LINK, W_COUNT
};
const FieldDef WIN_FIELDS[W_COUNT] = {
    { "UP TIME",   RANGE_TIME_MIN, RANGE_TIME_MAX, CS_STEP, F_TIME },
    { "DOWN TIME", RANGE_TIME_MIN, RANGE_TIME_MAX, CS_STEP, F_TIME },
    { "OVERLAP",   0, RANGE_OVERLAP_MAX, CS_STEP, F_TIME },
    { "BACKOFF",   0, RANGE_BACKOFF_MAX, CS_STEP, F_TIME },
    { "LIMIT",     PCT_MIN, PCT_MAX, PCT_STEP, F_PCT },
    { "I MAX UP",  RANGE_IMAX_MIN, RANGE_IMAX_MAX, 100, F_AMPS },
    { "I MAX DN",  RANGE_IMAX_MIN, RANGE_IMAX_MAX, 100, F_AMPS },
    { "OVL RATE",  RANGE_OVL_MIN, RANGE_OVL_MAX, 1, F_RATE },
    { "MAX RUN",   RANGE_MAXRUN_MIN, RANGE_MAXRUN_MAX, 1, F_MAXRUN },
    { "REHOME",    0, RANGE_REHOME_MAX, 1, F_COUNT },
    { "HOME TIME", 0, RANGE_HOME_MAX, CS_STEP, F_TIME },
    { "HOME SENS", 0, 1, 1, F_SENS },
    { "LINK",      0, 1, 1, F_ONOFF },
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

// 평소 화면에서 고른 항목이 곧 모드다. 온도는 자동, 나머지는 수동 (스펙 7절)
enum class Item : uint8_t { TEMP, SYNC, W1, W2 };
// 저장하는 값: 온도 0, 위치 동기 1. 개별 항목은 위치 동기로 돌아가므로 1로 저장
const uint8_t SAVED_TEMP = 0, SAVED_SYNC = 1;

enum class Glyphs : uint8_t { NONE, BIG, WIDE };

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
    void drawSmall(uint32_t now);
    void drawBig(uint32_t now);
    void bigDecimal(int8_t digit);
    void drawWide(uint32_t now);
    void useGlyphs(Glyphs g);
    bool hasTemp() const { return _st.temp.found; }
    bool anyMoving() const;
    bool anyFault() const;
    bool errorScreen() const;
    bool alarmOn() const;
    void logLcd(const char* what) const;
    void ackOne();
    void selectItem(Item it, bool user, uint32_t now);
    void sendMode();
    bool canMove(uint8_t w) const;
    uint8_t syncMask() const;
    pct_t startPct(uint8_t w) const;
    void cancelEdit();
    void commitPos(uint32_t now);
    bool blinkSel(uint32_t now) const { return now % BLINK_SEL_MS < BLINK_SEL_ON_MS; }
    bool blinkEdit(uint32_t now) const { return (now / BLINK_MS) % 2 == 0; }

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
    bool     _dark = false;       // 조작이 없고 알릴 것도 없어 꺼져 있음 (첫 조작은 화면만 켬)
    bool     _alarmWas = false;
    uint32_t _alarmSince = 0;
    uint8_t  _blMode = 0xFF;      // 시리얼 확인용: 0 꺼짐, 1 켜짐, 2 깜빡임
    int8_t   _pendDir = 0;
    uint32_t _pendMs = 0;
    bool     _burst = false;
    uint32_t _lastLcdInit = 0;

    // 평소 화면
    Item     _item = Item::SYNC;
    uint8_t  _savedItem = SAVED_SYNC;
    int8_t   _modeSent = -1;      // 제어 쪽에 알린 모드 (1 수동, 0 자동)
    uint32_t _selMs = 0;          // 마지막 조작 (개별 항목에서 5초 뒤 위치 동기로)
    bool     _editTemp = false;
    int16_t  _editSetT = 0;
    bool     _editPos = false;
    uint8_t  _editMask = 0;       // 입력 중인 창 (비트 0, 1)
    pct_t    _edit[2] = { 0, 0 };
    pct_t    _editBase[2] = { 0, 0 };
    uint32_t _editMs = 0;
    bool     _indivWait = false;  // 개별 이동을 넣음: 도착하면 위치 동기로
    bool     _indivSeen = false;
    uint32_t _indivMs = 0;
    Glyphs   _glyphs = Glyphs::NONE;
    Fmt::WideSlots _wide = { { -1, -1, -1, -1 } };
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
    // 마지막으로 고른 항목으로 시작한다. 수동이었으면 수동으로 (스펙 7절)
    _savedItem = Store::loadUiItem(SAVED_SYNC);
    _item = _savedItem == SAVED_TEMP ? Item::TEMP : Item::SYNC;
    sendMode();
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

// 작은 글씨 화면으로 바꿀 고장이나 에러가 있는지. 온도 센서 에러는 --.-C로만 알린다 (스펙 7절)
bool Ui::errorScreen() const {
    if (anyFault()) return true;
    for (uint8_t i = 0; i < _log.count(); i++) {
        const ErrorEntry& e = _log.at(i);
        if (!e.acked && e.cause != Cause::TEMP_LOST && e.cause != Cause::TEMP_BAD) return true;
    }
    return false;
}

// 백라이트를 깜빡일 것: 고장, 에러, 비상정지
bool Ui::alarmOn() const {
    return anyFault() || _log.hasUnacked() || _st.estop;
}

// 시리얼 확인용: 화면을 찾았는지, 초기화가 됐는지
void Ui::logLcd(const char* what) const {
    if (_lcd.addr() == 0) {
        Serial.printf("[lcd] %s: no display\n", what);
        return;
    }
    Serial.printf("[lcd] %s 0x%02X %s %s, backlight %s\n", what, _lcd.addr(),
                  _lcd.isOled() ? "OLED" : "LCD", _lcd.ok() ? "OK" : "FAIL", _backlight ? "ON" : "OFF");
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

    // 백라이트: 조작하면 켜고 5분 뒤 끈다. 창이 움직이는 것만으로는 켜지 않는다.
    // 고장, 에러, 비상정지가 있으면 끄지 않고 1초 간격으로 깜빡인다 (스펙 7절)
    bool alarm = alarmOn();
    if (alarm && !_alarmWas) _alarmSince = now;
    _alarmWas = alarm;
    bool active = _screen == Screen::SPLASH || now - _lastInputMs < BACKLIGHT_OFF_MS;
    _dark = !active && !alarm;
    _backlight = active || (alarm && ((now - _alarmSince) / BACKLIGHT_ALARM_MS) % 2 == 0);
    uint8_t mode = active ? 1 : (alarm ? 2 : 0);
    if (mode != _blMode) {
        _blMode = mode;
        Serial.printf("[lcd] backlight %s\n", mode == 1 ? "ON" : (mode == 2 ? "BLINK (fault/error/estop)" : "OFF"));
    }
    _lcd.setBacklight(_backlight);
    sendMode();

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
    // 빨리 돌리면 위치는 5, 10씩, 시간은 5초, 10초씩. 다른 값은 5배, 10배
    if (base == PCT_STEP) {
        if (gap < ENC_FASTER_MS) return 100;
        if (gap < ENC_FAST_MS) return 50;
        return base;
    }
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
        bool wasDark = _dark;
        _lastInputMs = now;
        if (wasDark) {
            // 꺼진 화면의 첫 조작은 화면만 켠다. 깜빡이는 동안은 꺼진 것으로 보지 않는다
            Serial.println("[lcd] backlight ON (input)");
            _dark = false;
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

void Ui::sendMode() {
    int8_t manual = _item == Item::TEMP ? 0 : 1;
    if (manual == _modeSent) return;
    Alert a = { manual ? AlertType::MODE_MANUAL : AlertType::MODE_AUTO, 0xFF, Cause::NONE };
    if (ControlTask::sendAlert(a)) _modeSent = manual;      // 못 보냈으면 다음 주기에 다시
}

// 항목을 바꾼다. user가 true면 사용자가 고른 것이라 저장한다
void Ui::selectItem(Item it, bool user, uint32_t now) {
    cancelEdit();
    _item = it;
    _selMs = now;
    _indivWait = false;
    sendMode();
    if (user) {
        uint8_t v = it == Item::TEMP ? SAVED_TEMP : SAVED_SYNC;
        if (v != _savedItem && Store::saveUiItem(v)) _savedItem = v;
    }
}

void Ui::cancelEdit() {
    _editTemp = false;
    _editPos = false;
    _editMask = 0;
}

bool Ui::canMove(uint8_t w) const {
    const WindowStatus& ws = _st.win[w];
    return ws.configured && ws.state != WinState::FAULT && ws.state != WinState::ESTOP
        && ws.state != WinState::CALIBRATING;
}

// 위치 동기에서 움직일 수 있는 창: 고장, 설정이 비어 있음, LINK 꺼짐은 건너뛴다
uint8_t Ui::syncMask() const {
    uint8_t m = 0;
    for (uint8_t w = 0; w < 2; w++)
        if (canMove(w) && _s.win[w].link) m |= (uint8_t)(1 << w);
    return m;
}

// 입력을 시작할 값: 움직이는 중이면 목표, 아니면 지금 위치. 0.5 단위로 맞춘다
pct_t Ui::startPct(uint8_t w) const {
    const WindowStatus& ws = _st.win[w];
    pct_t p;
    if (isMovingState(ws.state) && ws.state != WinState::CALIBRATING) p = Fmt::targetPct(ws);
    else p = Fmt::winPct(ws);
    if (p <= PCT_CLOSE) return PCT_CLOSE;                  // 모름도 CLOSE에서 시작
    p = (pct_t)((p + PCT_STEP / 2) / PCT_STEP * PCT_STEP);
    if (p < PCT_MIN) p = PCT_MIN;
    pct_t top = _s.win[w].limitPct;
    if (p > top) p = top;
    return p;
}

void Ui::mainShort(uint32_t now) {
    // 순서: 움직이면 정지, 다음이 에러 확인, 그다음 입력 취소나 다음 항목
    if (anyMoving()) {
        Alert a = { AlertType::STOP_ALL, 0xFF, Cause::NONE };
        ControlTask::sendAlert(a);
        cancelEdit();
        _jobPending = false;
        if (_item == Item::TEMP) selectItem(Item::SYNC, true, now);   // 자동으로 움직이던 것을 세우면 수동으로
        return;
    }
    if (_log.hasUnacked() || _st.win[0].state == WinState::FAULT || _st.win[1].state == WinState::FAULT) {
        ackOne();
        return;
    }
    if (_editTemp || _editPos) {
        cancelEdit();
        if (_item == Item::W1 || _item == Item::W2) selectItem(Item::SYNC, false, now);
        return;
    }
    Item next;
    switch (_item) {
    case Item::TEMP: next = Item::SYNC; break;
    case Item::SYNC: next = Item::W1; break;
    case Item::W1:   next = Item::W2; break;
    default:         next = hasTemp() ? Item::TEMP : Item::SYNC; break;
    }
    selectItem(next, true, now);
}

void Ui::mainSteps(const int8_t* dirs, const uint32_t* gaps, uint8_t n, uint32_t now) {
    if (!_st.bootDone) return;
    if (errorScreen()) { _snd.play(Snd::ERROR_BEEP); return; }   // 에러를 먼저 확인한다
    _selMs = now;
    bool changed = false, bumped = false;

    if (_item == Item::TEMP) {
        // 설정 온도: 0.5도씩
        if (!_editTemp) { _editTemp = true; _editSetT = _s.temp.setT; }
        for (uint8_t i = 0; i < n; i++) {
            int32_t v = _editSetT + dirs[i] * 5;
            if (v < 0 || v > 500) { bumped = true; continue; }
            _editSetT = (int16_t)v;
            changed = true;
        }
    } else {
        if (!_editPos) {
            uint8_t mask;
            if (_item == Item::SYNC) mask = syncMask();
            else {
                uint8_t w = _item == Item::W1 ? 0 : 1;
                mask = canMove(w) ? (uint8_t)(1 << w) : 0;
            }
            if (!mask) { _snd.play(Snd::ERROR_BEEP); return; }    // 움직일 창이 없음
            _editPos = true;
            _editMask = mask;
            for (uint8_t w = 0; w < 2; w++) _edit[w] = _editBase[w] = startPct(w);
        }
        // 위치 동기는 두 창에 같은 증감을 더한다. 닫을 때 낮은 쪽이 0.0이 되어도 높은 쪽은 계속 내려가고,
        // 열 때 높은 쪽이 리밋에서 멈춰도 낮은 쪽은 계속 올라간다
        for (uint8_t i = 0; i < n; i++) {
            pct_t step = (pct_t)stepSize(PCT_STEP, gaps[i]);
            bool any = false;
            for (uint8_t w = 0; w < 2; w++) {
                if (!((_editMask >> w) & 1)) continue;
                pct_t v = _edit[w], top = _s.win[w].limitPct;
                if (dirs[i] > 0) {
                    if (v == PCT_CLOSE) v = PCT_MIN;                 // CLOSE에서 한 칸 위는 1.0
                    else if (v >= top) continue;
                    else { v = (pct_t)(v + step); if (v > top) v = top; }
                } else {
                    if (v == PCT_CLOSE) continue;
                    if (v <= PCT_MIN) v = PCT_CLOSE;                 // 1.0 아래로 한 칸 더 돌리면 CLOSE
                    else { v = (pct_t)(v - step); if (v < PCT_MIN) v = PCT_MIN; }
                }
                _edit[w] = v;
                any = true;
            }
            if (any) changed = true; else bumped = true;
        }
    }
    _editMs = now;
    if (changed) _snd.play(Snd::CLICK);          // 한 주기에 여러 칸이어도 한 번
    else if (bumped) _snd.play(Snd::BUMP);
}

// 입력 완료: 바뀐 창만 움직인다. 위치 동기는 창 칸이 "양쪽"인 잡 하나
void Ui::commitPos(uint32_t now) {
    uint8_t mask = 0;
    cs_t t[2] = { 0, 0 };
    for (uint8_t w = 0; w < 2; w++) {
        if (!((_editMask >> w) & 1) || _edit[w] == _editBase[w]) continue;
        mask |= (uint8_t)(1 << w);
        t[w] = _edit[w] == PCT_CLOSE ? -1 : SettingsRules::posFromPct(_edit[w], _st.win[w].maxPos);
    }
    _editPos = false;
    _editMask = 0;
    if (!mask) return;
    Job j = {};
    j.source = JobSource::ENCODER;
    if (_item == Item::SYNC) {
        j.type = JobType::MOVE;
        j.window = JOB_BOTH;
        j.bothMask = mask;
        j.target = t[0];
        j.target2 = t[1];
    } else {
        uint8_t w = mask & 1 ? 0 : 1;
        j.window = w;
        if (t[w] < 0) j.type = JobType::CLOSE;
        else { j.type = JobType::MOVE; j.target = t[w]; }
        _indivWait = true;
        _indivSeen = false;
        _indivMs = now;
    }
    _pendingJob = j;
    _jobPending = !ControlTask::sendJob(j);
    _snd.play(Snd::COMMIT);
}

void Ui::mainTick(uint32_t now) {
    // 온도 센서가 없으면 온도 항목이 없다 (저장한 값은 그대로 둔다)
    if (_item == Item::TEMP && _st.bootDone && !hasTemp()) selectItem(Item::SYNC, false, now);

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
    if (_editPos && now - _editMs >= INPUT_COMMIT_MS) commitPos(now);

    // 개별 항목: 개별 이동이 도착하거나, 움직이지 않는 채로 5초 동안 조작이 없으면 위치 동기로
    if ((_item == Item::W1 || _item == Item::W2) && !_editPos) {
        uint8_t w = _item == Item::W1 ? 0 : 1;
        bool moving = isMovingState(_st.win[w].state);
        if (_indivWait) {
            if (moving) _indivSeen = true;
            else if (_indivSeen) { selectItem(Item::SYNC, false, now); return; }
            else if (now - _indivMs > 3000) _indivWait = false;      // 움직이지 않았음 (이미 그 자리 등)
        }
        if (!moving && !_indivWait && now - _selMs >= SYNC_RETURN_MS) selectItem(Item::SYNC, false, now);
    }
}

// 사용자 글자를 화면에 맞게 넣는다. 큰 글씨는 획 6개와 소수 자리 반쪽 2개, 개별 화면은 숫자 반쪽 8개
void Ui::useGlyphs(Glyphs g) {
    if (g == Glyphs::BIG) {
        for (uint8_t i = 0; i < Fmt::BIG_GLYPH_COUNT; i++) _lcd.setGlyph(i, Fmt::BIG_GLYPHS[i]);
    } else if (g == Glyphs::WIDE && _glyphs != Glyphs::WIDE) {
        for (uint8_t p = 0; p < 4; p++) _wide.digit[p] = -1;      // 다른 글자로 덮였으므로 처음부터
    }
    _glyphs = g;
}

void Ui::drawMain(uint32_t now) {
    bool homing = _st.win[0].state == WinState::HOMING || _st.win[1].state == WinState::HOMING;
    // 확인하지 않은 고장이나 에러, 원점 찾기는 작은 글씨 화면 (입력 중이면 입력 화면을 보인다)
    if (errorScreen() || (homing && !_editPos && !_editTemp)) drawSmall(now);
    else if (_item == Item::TEMP || _item == Item::SYNC) drawBig(now);
    else drawWide(now);
}

void Ui::drawSmall(uint32_t now) {
    bool blinkOn = blinkEdit(now);
    char line[17];
    for (uint8_t i = 0; i < 2; i++) {
        bool sensorErr = _log.hasUnacked((uint8_t)(i + 1), Cause::HOME_SENSOR);
        Fmt::smallLine(line, _st.win[i], (uint8_t)(i + 1), blinkOn, sensorErr);
        _lcd.setLine(i, line);
    }
}

// 큰 글씨의 소수 자리 숫자 반쪽을 사용자 글자 6, 7번에 넣는다 (바뀐 것만 보낸다)
void Ui::bigDecimal(int8_t digit) {
    uint8_t g[8];
    Fmt::bigDecHalf(digit, 0, g); _lcd.setGlyph(Fmt::BIG_DEC_SLOT, g);
    Fmt::bigDecHalf(digit, 1, g); _lcd.setGlyph(Fmt::BIG_DEC_SLOT + 1, g);
}

void Ui::drawBig(uint32_t now) {
    useGlyphs(Glyphs::BIG);
    bool st = blinkEdit(now);                 // 상태 글자 0.5초
    char r0[17], r1[17];
    memset(r0, ' ', 16); memset(r1, ' ', 16);
    r0[16] = 0; r1[16] = 0;
    bool moving = anyMoving();
    char s1 = Fmt::statusChar(_st.win[0], st), s2 = Fmt::statusChar(_st.win[1], st);

    // 오른쪽 위: 현재 온도. 끊기면 --.-C를 깜빡인다
    if (hasTemp()) {
        char t[8];
        if (_st.temp.valid) { Fmt::temp4(t, _st.temp.temp); memcpy(r0 + 11, t, 4); r0[15] = 'C'; }
        else if (st) memcpy(r0 + 11, "--.-C", 5);
    }

    if (_item == Item::TEMP) {
        bool show = _editTemp ? blinkEdit(now) : blinkSel(now);
        if (show) bigDecimal(Fmt::bigNumber(r0, r1, _editTemp ? _editSetT : _s.temp.setT));
        if (moving) { r1[12] = '1'; r1[13] = s1; r1[14] = '2'; r1[15] = s2; }
        else memcpy(r1 + 13, "SET", 3);
    } else {
        // 위치 동기: 낮은 쪽 창의 값. 두 창이 다르면 *
        uint8_t mask = _editPos ? _editMask : syncMask();
        if (!mask) mask = 3;
        pct_t v[2];
        for (uint8_t w = 0; w < 2; w++) v[w] = _editPos ? _edit[w] : Fmt::winPct(_st.win[w]);
        pct_t low = PCT_MAX + 1;
        bool unknown = false;
        for (uint8_t w = 0; w < 2; w++) {
            if (!((mask >> w) & 1)) continue;
            if (v[w] < 0) unknown = true;
            else if (v[w] < low) low = v[w];
        }
        if (unknown) low = PCT_UNKNOWN;
        bool differ = mask == 3 && v[0] != v[1];
        // 움직이는 중에는 현재 위치라 깜빡이지 않는다
        bool show = _editPos ? blinkEdit(now) : (moving ? true : blinkSel(now));
        if (show) bigDecimal(Fmt::bigNumber(r0, r1, low));
        if (hasTemp()) {
            memcpy(r1 + 11, "MAN", 3);
            if (moving) { r1[14] = s1; r1[15] = s2; }
            else if (differ) r1[14] = Fmt::G_DIFF;
        } else {
            memcpy(r0 + 13, "1+2", 3);
            if (moving) { r1[14] = s1; r1[15] = s2; }
            else if (differ) r1[15] = Fmt::G_DIFF;
        }
    }
    _lcd.setLine(0, r0);
    _lcd.setLine(1, r1);
}

// 개별 화면: 윗줄 창1, 아랫줄 창2. 고른 창의 값만 깜빡인다
void Ui::drawWide(uint32_t now) {
    useGlyphs(Glyphs::WIDE);
    uint8_t sel = _item == Item::W1 ? 0 : 1;
    bool st = blinkEdit(now);
    pct_t v[2];
    bool need[10] = {};
    for (uint8_t w = 0; w < 2; w++) {
        v[w] = (_editPos && ((_editMask >> w) & 1)) ? _edit[w] : Fmt::winPct(_st.win[w]);
        Fmt::wideNeed(need, v[w]);
    }
    Fmt::wideAssign(_wide, need);
    for (uint8_t p = 0; p < 4; p++) {
        if (_wide.digit[p] < 0) continue;
        uint8_t g[8];
        Fmt::wideHalf((uint8_t)_wide.digit[p], 0, g); _lcd.setGlyph((uint8_t)(2 * p), g);
        Fmt::wideHalf((uint8_t)_wide.digit[p], 1, g); _lcd.setGlyph((uint8_t)(2 * p + 1), g);
    }

    char line[17], extra[8];
    for (uint8_t w = 0; w < 2; w++) {
        const WindowStatus& ws = _st.win[w];
        bool editing = _editPos && ((_editMask >> w) & 1);
        bool mine = w == sel;
        WinState s = ws.state;
        bool moving = s == WinState::MOVING || s == WinState::CLOSING || s == WinState::REVERSE_WAIT;
        memset(extra, ' ', 7);
        extra[7] = 0;
        extra[6] = Fmt::statusChar(ws, st);
        if (!editing && moving && v[w] >= 0) {
            // 목표와 방향. 고른 창이면 목표가 깜빡인다
            if (!mine || blinkSel(now)) {
                char t[8];
                Fmt::pct4(t, Fmt::targetPct(ws));
                extra[0] = '>';
                memcpy(extra + 1, t, 4);
            }
        } else if (!editing && v[w] == PCT_CLOSE) {
            memcpy(extra, "CLOSE", 5);
        }
        bool show = true;
        if (mine) show = editing ? blinkEdit(now) : (moving ? true : blinkSel(now));
        Fmt::wideLine(line, (uint8_t)(w + 1), v[w], _wide, show, extra);
        _lcd.setLine(w, line);
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
    cancelEdit();
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
    _selMs = millis();
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
        case W_LIMIT: return w.limitPct;
        case W_IMAXUP: return w.iMaxUp;
        case W_IMAXDN: return w.iMaxDn;
        case W_OVL: return w.ovlRate;
        case W_MAXRUN: return w.maxRunS;
        case W_REHOME: return w.rehome;
        case W_HOMETIME: return w.homeTime;
        case W_HOMESENS: return w.homeSensAuto ? 1 : 0;
        default: return w.link ? 1 : 0;
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
        case W_LIMIT: w.limitPct = (pct_t)v; break;
        case W_IMAXUP: w.iMaxUp = v; break;
        case W_IMAXDN: w.iMaxDn = v; break;
        case W_OVL: w.ovlRate = (uint8_t)v; break;
        case W_MAXRUN: w.maxRunS = (int16_t)v; break;
        case W_REHOME: w.rehome = (uint8_t)v; break;
        case W_HOMETIME: w.homeTime = v; break;
        case W_HOMESENS: w.homeSensAuto = v != 0; break;
        default: w.link = v != 0; break;
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
    case F_PCT:
        if (v >= PCT_MAX) strcpy(out, "OFF");
        else { Fmt::pct4(b, (pct_t)v); snprintf(out, 16, "%s", b[0] == ' ' ? b + 1 : b); }
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

    if (f.fmt == F_PCT) {
        // 1.0~99.9를 0.5씩. 99.9(OFF)에서 내리면 99.5부터
        if (v < f.min || v > f.max) return f.max;
        int32_t n = (v == f.max && dir < 0) ? 1000 - mul : v + dir * mul;
        if (n < f.min) { bumped = v == f.min; n = f.min; }
        if (n > f.max) { bumped = v == f.max; n = f.max; }
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
        if (ws.limitPct < PCT_MAX) { message("CALIBRATE", "LIMIT IS ON", now, false, Level::CAL_SELECT); break; }
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
        snprintf(l1, sizeof(l1), "%s", TOP_ITEMS[_top]);
        break;
    case Level::LIST:
    case Level::EDIT: {
        char left[17];
        const char* prefix = _list == ListKind::WIN ? (_listWin == 0 ? "W1 " : "W2 ") : "";
        if (_idx >= listCount()) {
            const char* title = _list == ListKind::WIN ? (_listWin == 0 ? "WINDOW 1" : "WINDOW 2")
                              : (_list == ListKind::TEMP ? "TEMP CONTROL" : "SOUND");
            snprintf(l0, sizeof(l0), "%-16s", title);
            strcpy(l1, "BACK");
            break;
        }
        snprintf(left, sizeof(left), "%s%s", prefix, field(_idx).name);
        header(l0, left, (uint16_t)(_idx + 1), listCount());
        // 값을 바꾸는 중에는 값만 깜빡인다. 항목 이름은 깜빡이지 않는다 (스펙 7절)
        fmtField(b, _idx, _lv == Level::EDIT ? _val : getField(_s, _idx));
        if (_lv == Level::EDIT && !blinkOn) strcpy(l1, "");
        else snprintf(l1, sizeof(l1), "%s", b);
        break;
    }
    case Level::CAL_SELECT:
        snprintf(l0, sizeof(l0), "CALIBRATE");
        snprintf(l1, sizeof(l1), "%s", _idx == 0 ? "WINDOW 1" : (_idx == 1 ? "WINDOW 2" : "BACK"));
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
        // 고른 쪽이 깜빡인다
        strcpy(l1, "YES   NO");
        if (!blinkOn) memcpy(l1 + (_calYes ? 0 : 6), _calYes ? "   " : "  ", _calYes ? 3 : 2);
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
