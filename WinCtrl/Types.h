#pragma once
#include <stdint.h>
#include "config.h"

// ============================================================
//  공통 열거형
// ============================================================
enum class Model : uint8_t { W50, W300 };

// 원점 센서 입력: 있음(ON, 원점 높이 아래), 없음(OFF), 모름(전원 켠 직후 첫 판정 전)
enum class SensorState : uint8_t { UNKNOWN, PRESENT, ABSENT };

enum class WinState : uint8_t {
    IDLE, HOMING, MOVING, CLOSING, REVERSE_WAIT, CALIBRATING, LIMIT, FAULT, ESTOP
};

// 위치의 유효 여부는 숫자와 따로 둔다
enum class PosState : uint8_t { UNKNOWN, CLOSED, VALID };

// 고장과 에러 원인. 에러 저장(11절)의 원인 값으로도 쓴다.
enum class Cause : uint8_t {
    NONE = 0,
    OVERLOAD, NO_CURRENT, LIMIT_HIT, DRIVER, TIMEOUT, SENSOR, RELAY,  // 하드웨어 이상 (멈추는 것은 isStopCause만)
    HOME_SENSOR,                                                     // 원점 센서 에러 (센서 없이 계속)
    TEMP_LOST, TEMP_BAD                                              // 온도 센서 끊김, 이상
};

inline bool isFaultCause(Cause c) { return c >= Cause::OVERLOAD && c <= Cause::RELAY; }
// 동작을 멈추는 고장은 과전류뿐이다: 과부하, 드라이버 고장 신호. 나머지는 경고만 (스펙 6절, 2026-11-28)
inline bool isStopCause(Cause c) { return c == Cause::OVERLOAD || c == Cause::DRIVER; }

// ============================================================
//  잡 (화면/온도제어 → 제어)
// ============================================================
enum class JobType : uint8_t { MOVE, CLOSE, HOME, CALIBRATE, SETTINGS_CHANGED };
enum class JobSource : uint8_t { ENCODER, TEMP_CONTROL };

// 위치 동기(1+2)의 잡은 창 칸이 "양쪽"이다. 두 창 모두에 같은 주기에 넣는다 (스펙 2절)
const uint8_t JOB_BOTH = 2;

struct Job {
    JobType   type;
    JobSource source;
    uint8_t   window;      // 0, 1, JOB_BOTH. SETTINGS_CHANGED는 아래 마스크를 씀
    cs_t      target;      // MOVE의 목표. JOB_BOTH면 창1의 목표 (-1 = CLOSE)
    cs_t      target2;     // JOB_BOTH: 창2의 목표 (-1 = CLOSE)
    uint8_t   bothMask;    // JOB_BOTH: 움직일 창 (비트 0, 1)
    uint8_t   timesMask;   // SETTINGS_CHANGED: 시간 값이 바뀐 창 (비트 0, 1)
    uint8_t   calMask;     // SETTINGS_CHANGED: 측정 결과를 저장한 창
};

// ============================================================
//  얼럿 (화면 → 제어). 잡보다 먼저 처리한다.
// ============================================================
enum class AlertType : uint8_t {
    STOP_ALL,         // 전체 정지
    CLEAR_FAULT,      // 고장 해제 (window, 공통이면 0xFF)
    RECHECK_SENSOR,   // 원점 센서 에러 확인: 센서 판단을 처음으로
    RESTORE_FAULT,    // 전원 켤 때 미확인 고장으로 시작
    SENSOR_HISTORY,   // 이 창의 원점 센서 에러 기록이 있음
    MENU_ENTER,       // 설정 메뉴 들어감 (온도제어 멈춤)
    MENU_EXIT,
    MODE_MANUAL,      // 위치 항목을 고름: 수동, 온도제어 멈춤 (스펙 7절)
    MODE_AUTO         // 온도 항목을 고름: 자동
};

struct Alert {
    AlertType type;
    uint8_t   window;
    Cause     cause;
};

// ============================================================
//  설정 (플래시에 저장)
// ============================================================
// 위치는 bottom(내장 하한 리밋)에서 올라간 초로 센다 (스펙 3절, 2026-11-22).
// 모터 정보(상승/하강/원점 시간, 최대 운전 전류)가 비어 있으면 모델 기본값으로 움직인다 (스펙 8절).
struct WindowSettings {
    cs_t     upTime;       // 상승 시간 bottom → top (CS_EMPTY = 모델 기본값)
    cs_t     downTime;     // 하강 시간 top → bottom (CS_EMPTY = 모델 기본값)
    cs_t     homeTime;     // 원점 시간 bottom → home (CS_EMPTY = 모델 기본값)
    cs_t     backoff;      // 되내림 시간 (원점 센서를 켠 창)
    cs_t     sensorRun;    // 측정으로 잰 bottom → 센서 지점 (메뉴에 보이지 않음, 원점 시간 계산)
    int16_t  limitPct;     // 상한 리밋, 위치 백분율 0.1 단위 (PCT_MAX = 리밋 없음)
    int32_t  iMaxUp;       // 최대 운전 전류 mA, 넘으면 과부하 (-1 = 모델 기본값)
    int32_t  iMaxDn;
    int16_t  maxRunS;      // 최대 구동 시간 초 (-1 = 자동: 상승 시간의 1.2배)
    bool     homeSensAuto; // 원점 센서 사용: 자동(true) / 끔(false, 기본)
    bool     link;         // 위치 동기와 온도제어에 참여 (LINK)
};

struct TempSettings {
    bool     sensorAuto;   // 온도 센서 사용: 자동 / 끔
    int16_t  setT;         // 설정 온도 (0.1도)
    int16_t  deadband;     // 불감대 (0.1도)
    uint16_t periodS;      // 판단 주기 (초)
    cs_t     step;         // 한 단계
    cs_t     stepAdd;      // 단계 증가량 (0 = 끔)
    cs_t     stepMax;      // 최대 단계
    uint8_t  slopeMin;     // 기울기 구간 (분)
    uint8_t  aheadMin;     // 내다보는 시간 (분, 0 = 끔)
    int16_t  t4;           // 4mA 온도 (0.1도)
    int16_t  t20;          // 20mA 온도
    int16_t  offset;       // 온도 보정값
};

struct Settings {
    uint16_t       version;
    WindowSettings win[2];
    TempSettings   temp;
    bool           clickSound;
    bool           alarmSound;
    // 제품 구성 (스펙 8절, 2026-11-28). 기본 구성은 MC 없음, 원점 센서 없음, 온도 센서 없음
    bool           relayUse;     // 주 차단 MC 있음. 끄면 MC 자가 점검과 감시를 하지 않는다
    bool           basicAcked;   // 처음 켤 때의 구성 경고를 확인함 (한 번만 보인다)
};

// ============================================================
//  상태 복사본 (제어 → 화면, 나중에 통신 쪽도 읽음)
// ============================================================
enum class CalPhase : uint8_t { NONE, DOWN_FIRST, WAIT_1, UP, WAIT_2, DOWN, DONE, FAILED };

struct CalStatus {
    CalPhase phase;
    bool     sensorMode;   // 센서로 잰 측정인지
    cs_t     elapsed;      // 현재 단계의 경과 시간
    int32_t  currentMa;
    Cause    failCause;    // 실패 원인 (정지/비상정지로 중단되면 NONE)
    bool     outOfRange;   // 끝까지 갔지만 값이 허용 범위 밖
    // 결과 (phase == DONE). 최대 운전 전류는 잰 값 × 1.3 (저장할 값)
    cs_t     upTime, downTime;
    cs_t     sensorRun;    // bottom → 센서 지점 (센서로 잰 측정만, 아니면 CS_EMPTY)
    int32_t  iMaxUp, iMaxDn, iAvgUp, iAvgDn;
};

struct WindowStatus {
    WinState    state;
    PosState    posState;
    cs_t        pos;
    cs_t        target;         // 이동 중의 목표 (CLOSE면 -1)
    int8_t      dir;            // 구동 중 방향 (+1 상승, -1 하강, 0 정지)
    int8_t      nextDir;        // REVERSE_WAIT 뒤 방향
    Cause       fault;          // FAULT 원인
    bool        configured;     // 이동을 받을 수 있는 설정인지
    bool        defaults;       // 비어 있는 모터 정보를 모델 기본값으로 채워 쓰는 중
    bool        sensorFound;    // 원점 센서 "있음"으로 정해짐 (입력이 ON이 됨)
    bool        sensorAbsent;   // "없음"으로 정해짐 (OFF인 채로 하한). 둘 다 false면 아직 모름
    bool        sensorActive;   // 지금 센서로 원점을 잡는지
    bool        sensorError;    // 원점 센서 에러 표시 중
    bool        resync;         // 일찍 리밋에 닿아 끝점에서 위치를 다시 맞추는 중 (2026-11-26)
    SensorState sensor;
    cs_t        home;           // home 위치 (bottom = 0)
    cs_t        maxPos;         // top 위치
    cs_t        limit;          // 실제로 쓰는 상한 위치 (리밋 OFF면 top)
    int32_t     currentMa;
    int32_t     moveMaxMa;      // 마지막(또는 지금) 이동의 최대 전류 (0.3초 평균)
    int32_t     overloadUpMa;   // 과부하 기준
    int32_t     overloadDnMa;
    CalStatus   cal;
};

struct TempStatus {
    bool     found;        // 전원 켤 때 감지 (설정 끔이면 false)
    bool     valid;        // 지금 값이 유효한지
    int16_t  temp;         // 현재 온도 (0.1도)
    int16_t  predicted;    // 예측 온도
    int16_t  slope;        // 변화 속도 (분당 0.01도)
    uint16_t nextS;        // 다음 판단까지 남은 초 (화면 표시용)
    bool     paused;       // 설정 메뉴나 수동 때문에 멈춤
    bool     manual;       // 수동 (위치 항목을 고름)
};

// 제어 쪽에서 생긴 에러를 화면 태스크가 에러 저장에 넣도록 넘기는 고리
const uint8_t EVENT_RING_SIZE = 8;
struct ErrorEvent {
    uint8_t window;        // 1, 2, 0 = 공통
    Cause   cause;
};

struct Status {
    WindowStatus win[2];
    TempStatus   temp;
    Model        model;
    bool         bootDone;     // 자가 점검과 센서 판단이 끝났는지
    bool         estop;
    bool         relayUse;     // MC 있음 설정
    bool         relayFault;   // MC 경고가 한 번이라도 남 (운전은 막지 않음)
    bool         relayOn;      // MC FET A, B가 모두 켜짐 (디버그 표시용)
    int32_t      relayMv;      // MC 뒤 전압 (디버그 표시용)
    int32_t      estopMv;      // 비상 버튼 뒤 전압 (디버그 표시용)
    int32_t      tempUa;       // 온도 센서 전류 (디버그 표시용)
    uint32_t     eventCount;   // 지금까지 넣은 이벤트 수
    ErrorEvent   events[EVENT_RING_SIZE];
};

inline bool isMovingState(WinState s) {
    return s == WinState::HOMING || s == WinState::MOVING || s == WinState::CLOSING
        || s == WinState::REVERSE_WAIT || s == WinState::CALIBRATING;
}
