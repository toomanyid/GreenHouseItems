#pragma once
#include <stdint.h>
#include "Types.h"
#include "ctl_Safety.h"

// ============================================================
//  2층: 창 하나의 상태 흐름, 위치 계산, 구동 중 판정
//
//  하드웨어를 직접 부르지 않는다. 제어 태스크가 매 주기 잰 값을
//  WindowInputs로 넘기고, 돌려받은 WindowOutputs를 모터에 건다.
//  그래서 PC에서 가짜 값으로 시험할 수 있다.
//
//  상태는 enum 하나와 switch 하나로 처리한다 (스펙 4절 "상태 흐름 구현").
// ============================================================

struct WindowInputs {
    uint32_t    now;            // ms
    int32_t     currentMa;      // 이 창 모터 전류
    bool        driverFault;    // 전류 감지 전압이 클램프 수준
    SensorState sensor;         // 원점 센서 (0.1초 유지, 조합 이상 0.5초면 UNKNOWN)
    bool        contactsValid;  // 지금 순간 두 접점 중 하나만 닫혀 있는지
    bool        relayReady;     // 주 릴레이가 붙고 0.1초 지남
};

struct WindowOutputs {
    int8_t   dir;        // +1 상승, -1 하강, 0 끊음
    uint16_t duty;       // 0~1000
    bool     wantRelay;  // 주 릴레이가 필요함 (방향 전환 정지 중 포함)
};

class Window {
public:
    void begin(uint8_t index, Model model, uint32_t now);

    // 설정
    void setSettings(const WindowSettings& s) { _s = s; }
    void settingsChanged(const WindowSettings& s, bool timesChanged, bool calSaved);
    void setSensorFound(bool found)            { _sensorFound = found; }
    void setSensorHistory();                   // 이 창의 원점 센서 에러 기록이 있음 (전원 켤 때)

    // 잡과 얼럿
    bool submit(const Job& job);               // "다음 목표" 칸에 넣음. 받지 않으면 false
    void stop(uint32_t now);                   // 전체 정지 얼럿
    void clearFault(uint32_t now);
    void restoreFault(Cause c);                // 전원 켤 때 미확인 고장으로 시작
    void recheckSensor(uint32_t now);          // 원점 센서 에러 확인 뒤 3초 동안 다시 읽음
    void setEstop(bool on, uint32_t now);
    void forceFault(Cause c, uint32_t now);    // 릴레이 고장처럼 밖에서 정한 고장

    WindowOutputs tick(const WindowInputs& in);

    void fillStatus(WindowStatus& st) const;
    bool popEvent(Cause& c);                   // 에러 저장으로 넘길 일

    // 조정 층(온도제어)에서 쓰는 조회
    WinState state() const       { return _state; }
    PosState posState() const    { return _posState; }
    cs_t     pos() const         { return _pos; }
    cs_t     limitPos() const;
    cs_t     maxPos() const;
    bool     configured() const;
    bool     sensorActive() const { return _sensorFound && _s.homeSensAuto && !_sensorErrMode; }

private:
    enum class Goal : uint8_t { NONE, TARGET, CLOSE };
    enum class HomePhase : uint8_t { DOWN_TO_SENSOR, EXTRA_DOWN, UP, NOSENSOR_DOWN };
    enum class FaultPhase : uint8_t { HOLD, BACKOFF_WAIT, BACKOFF_RUN };

    // 목표와 움직임 시작
    void beginGoal(uint32_t now);
    void startState(WinState st, int8_t dir, uint32_t now);
    bool needWait(int8_t dir, uint32_t now) const;
    void goReverseWait(WinState resume, int8_t dir, uint32_t now);
    void startHoming(uint32_t now);
    bool needHoming() const;
    void applyPending(uint32_t now);
    void settleIdle();

    // 구동
    void startDrive(int8_t dir, uint32_t now);
    void cut(uint32_t now);
    void accumulate(uint32_t dt);

    // 판정
    void checkRunning(const WindowInputs& in);
    void onCurrentStopped(uint32_t now, SensorState sensor);
    void onNoCurrent(uint32_t now, SensorState sensor);
    void raiseFault(Cause c, uint32_t now);
    void overload(uint32_t now);
    void sensorFailure(uint32_t now);
    void handleSensor(const WindowInputs& in);
    void calFail(Cause c, uint32_t now);

    // 상태별 일
    void tickHoming(const WindowInputs& in);
    void tickMoving(const WindowInputs& in);
    void tickClosing(const WindowInputs& in);
    void tickReverseWait(const WindowInputs& in);
    void tickCalibrating(const WindowInputs& in);
    void tickFault(const WindowInputs& in);

    void     pushEvent(Cause c);
    cs_t     sensorPoint() const;
    uint32_t maxRunMs() const;
    int32_t  overloadMa(int8_t dir) const;
    int32_t  fixedLimitMa() const { return _model == Model::W300 ? FIXED_LIMIT_MA_300W : FIXED_LIMIT_MA_50W; }
    bool     limitEnabled() const { return _s.limit >= 0 && _s.limit < maxPos(); }
    cs_t     runCs() const        { return (cs_t)(_runRaw / 1000); }

    uint8_t        _index = 0;
    Model          _model = Model::W50;
    WindowSettings _s = {};

    WinState  _state = WinState::IDLE;
    PosState  _posState = PosState::UNKNOWN;
    cs_t      _pos = 0;
    int32_t   _acc = 0;

    // 목표
    Goal      _goal = Goal::NONE;
    cs_t      _target = 0;
    JobSource _goalSource = JobSource::ENCODER;
    bool      _pendingValid = false;
    Job       _pending = {};

    // 구동
    int8_t    _dir = 0;
    uint16_t  _duty = 0;
    bool      _waitRelay = false;
    uint32_t  _driveStartMs = 0;
    int64_t   _runRaw = 0;          // 이번 구동에서 쌓인 100% 출력 환산 시간 (1000 = 1cs, 환산 없음)
    int8_t    _lastCutDir = 0;
    uint32_t  _lastCutMs = 0;
    uint32_t  _bootGuardMs = 0;
    uint32_t  _lastTickMs = 0;
    bool      _ticked = false;
    CurrentMonitor _mon;
    int32_t   _moveMax = 0;
    int32_t   _currentMa = 0;
    uint16_t  _startsWithoutPass = 0;
    bool      _upPassActive = false;
    cs_t      _upPassStart = 0;

    // 방향 전환 정지
    WinState  _resume = WinState::IDLE;
    int8_t    _resumeDir = 0;

    // 원점 잡기와 닫기
    HomePhase _hPhase = HomePhase::DOWN_TO_SENSOR;
    uint32_t  _phaseMs = 0;
    bool      _closeSensorSeen = false;
    cs_t      _closeSensorRun = 0;
    bool      _passedThisTick = false;

    // 고장
    Cause      _fault = Cause::NONE;
    FaultPhase _fPhase = FaultPhase::HOLD;
    int8_t     _backoffDir = 0;
    uint32_t   _clearMs = 0;
    bool       _everCleared = false;

    // 원점 센서
    bool        _sensorFound = false;
    bool        _sensorErrMode = false;   // 센서 없는 것처럼 동작 중
    SensorState _lastSensor = SensorState::UNKNOWN;
    bool        _recheck = false;
    bool        _recheckBad = false;
    uint32_t    _recheckMs = 0;

    // 이동 시간 측정
    CalStatus _cal = {};
    bool      _calSensor = false;
    bool      _calMark = false;           // 이번 단계에서 센서 전환을 봤는지
    cs_t      _calMarkRun = 0;

    // 에러 이벤트
    Cause     _events[4] = {};
    uint8_t   _evCount = 0;
};
