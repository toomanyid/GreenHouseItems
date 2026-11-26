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
    SensorState sensor;         // 원점 센서 ON = PRESENT, OFF = ABSENT (0.1초 유지, 처음 판정 전 UNKNOWN)
    bool        relayReady;     // 주 차단 MC가 붙고 0.1초 지남
};

struct WindowOutputs {
    int8_t   dir;        // +1 상승, -1 하강, 0 끊음
    uint16_t duty;       // 0~1000
    bool     wantRelay;  // 주 차단 MC가 필요함 (방향 전환 정지 중 포함)
};

class Window {
public:
    void begin(uint8_t index, Model model, uint32_t now);

    // 설정
    void setSettings(const WindowSettings& s) { useSettings(s); }
    void settingsChanged(const WindowSettings& s, bool timesChanged, bool calSaved);
    void setSensorHistory() { _sensorHistory = true; }   // 이 창의 원점 센서 에러 기록이 있음 (전원 켤 때)

    // 잡과 얼럿
    bool submit(const Job& job);               // "다음 목표" 칸에 넣음. 받지 않으면 false
    void stop(uint32_t now);                   // 전체 정지 얼럿
    void clearFault(uint32_t now);
    void restoreFault(Cause c);                // 전원 켤 때 미확인 고장으로 시작
    void recheckSensor();                      // 원점 센서 에러 확인: 센서 판단을 처음으로 돌림
    void setEstop(bool on, uint32_t now);
    void forceFault(Cause c, uint32_t now);    // MC 고장처럼 밖에서 정한 고장

    WindowOutputs tick(const WindowInputs& in);

    void fillStatus(WindowStatus& st) const;
    bool popEvent(Cause& c);                   // 에러 저장으로 넘길 일

    // 조정 층(온도제어)에서 쓰는 조회
    WinState state() const       { return _state; }
    PosState posState() const    { return _posState; }
    cs_t     pos() const         { return _pos; }      // bottom = 0 (스펙 3절)
    cs_t     home() const        { return _s.homeTime; }
    cs_t     limitPos() const;
    cs_t     maxPos() const;                           // top
    bool     configured() const;
    // 센서로 원점을 잡는지. 설정이 "자동"이고 아직 정해지지 않았으면 있는 것처럼 잡는다 (스펙 10절)
    bool     sensorActive() const { return _s.homeSensAuto && _known != Known::ABSENT && !_sensorErrMode; }

private:
    enum class Known : uint8_t { UNDECIDED, PRESENT, ABSENT };
    void decideAbsentAtBottom(uint32_t now);
    void calAtBottom(SensorState sensor, uint32_t now);

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
    void tickReverseWait(const WindowInputs& in);
    void tickCalibrating(const WindowInputs& in);
    void tickFault(const WindowInputs& in);

    void     useSettings(const WindowSettings& s);   // 저장값을 받고 빈 항목은 모델 기본값으로 채운다
    void     pushEvent(Cause c);
    cs_t     sensorPoint() const;
    uint32_t maxRunMs() const;
    int32_t  overloadMa(int8_t dir) const;
    int32_t  fixedLimitMa() const { return _model == Model::W300 ? FIXED_LIMIT_MA_300W : FIXED_LIMIT_MA_50W; }
    void     startUpPass(cs_t base) { _upPassActive = true; _upPassStart = runCs(); _upPassBase = base; }
    bool     limitEnabled() const { return _s.limitPct < PCT_MAX && limitPos() < maxPos(); }
    cs_t     runCs() const        { return (cs_t)(_runRaw / 1000); }

    uint8_t        _index = 0;
    Model          _model = Model::W50;
    WindowSettings _raw = {};       // 받은 설정 (빈 항목 그대로)
    WindowSettings _s = {};         // 실제로 쓰는 설정 (빈 항목은 모델 기본값)
    bool           _defaults = false; // 기본값을 하나라도 쓰는 중

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
    int8_t    _atLimit = 0;             // 내장 리밋에 닿아 선 쪽 (-1 하한, +1 상한, 0 아님)
    bool      _releasing = false;       // 이번 구동이 리밋에서 반대로 떠나는 구동인지
    bool      _resync = false;          // 올라가다 일찍 리밋에 닿아 끝점에서 다시 맞추고 목표로 가는 중 (2026-11-26)
    uint32_t  _lastCutMs = 0;
    uint32_t  _bootGuardMs = 0;
    uint32_t  _lastTickMs = 0;
    bool      _ticked = false;
    CurrentMonitor _mon;
    int32_t   _moveMax = 0;
    int32_t   _currentMa = 0;
    bool      _upPassActive = false;    // bottom이나 센서 지점에서 연속으로 올리는 중 (리밋 세 번째 겹)
    cs_t      _upPassStart = 0;
    cs_t      _upPassBase = 0;

    // 방향 전환 정지
    WinState  _resume = WinState::IDLE;
    int8_t    _resumeDir = 0;

    // 원점 잡기와 닫기
    HomePhase _hPhase = HomePhase::DOWN_TO_SENSOR;
    uint32_t  _phaseMs = 0;
    bool      _passedThisTick = false;

    // 고장
    Cause      _fault = Cause::NONE;
    FaultPhase _fPhase = FaultPhase::HOLD;
    int8_t     _backoffDir = 0;
    uint32_t   _clearMs = 0;
    bool       _everCleared = false;

    // 원점 센서
    // 입력이 on/off 하나라 ON이 들어오면 "있음", OFF인 채로 처음 하한에 닿으면 "없음"으로 정한다
    Known       _known = Known::UNDECIDED;
    bool        _sensorErrMode = false;   // 센서 이상으로 센서 없는 것처럼 동작 중
    bool        _sensorHistory = false;   // 이 창의 원점 센서 에러 기록이 있음
    SensorState _lastSensor = SensorState::UNKNOWN;

    // 이동 시간 측정
    CalStatus _cal = {};
    bool      _calSensor = false;
    bool      _calMark = false;           // 이번 단계에서 센서 전환을 봤는지
    cs_t      _calMarkRun = 0;

    // 에러 이벤트
    Cause     _events[4] = {};
    uint8_t   _evCount = 0;
};
