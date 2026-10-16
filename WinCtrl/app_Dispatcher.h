#pragma once
#include <stdint.h>
#include "Types.h"
#include "ctl_Window.h"
#include "ctl_Safety.h"
#include "app_TempControl.h"

// ============================================================
//  3층: 조정. 얼럿과 잡을 받아 창에 넘기고, 창 2개와 릴레이,
//  비상정지, 온도제어를 한 주기씩 진행한다.
//  하드웨어를 부르지 않는다. app_ControlTask가 잰 값을 넘기고 출력을 건다.
// ============================================================

struct ControlInputs {
    uint32_t    now;
    int32_t     currentMa[2];
    bool        driverFault[2];
    SensorState sensor[2];
    int32_t     relayMv;        // 릴레이 뒤 24V 쪽 전압
    int32_t     estopMv;        // 비상 버튼 뒤 24V 쪽 전압
    int32_t     tempUa;         // 온도 센서 전류 (µA)
};

struct ControlOutputs {
    int8_t   dir[2];
    uint16_t duty[2];
    bool     fetA;
    bool     fetB;
};

class Dispatcher {
public:
    void begin(Model model, const Settings& s, uint32_t now);

    void alert(const Alert& a, uint32_t now);
    bool job(const Job& j, uint32_t now);
    void settingsChanged(const Settings& s, uint8_t timesMask, uint8_t calMask);

    ControlOutputs step(const ControlInputs& in);
    void fillStatus(Status& st) const;

    bool bootDone() const { return _bootDone; }
    const Window& window(uint8_t i) const { return _w[i]; }

private:
    void bootStep(const ControlInputs& in);
    void pushEvent(uint8_t window, Cause c);

    Model       _model = Model::W50;
    Settings    _s = {};
    Window      _w[2];
    RelayControl _relay;
    TempControl _temp;
    HoldFilter  _estop;

    uint32_t    _startMs = 0;
    bool        _bootDone = false;
    int64_t     _tempSum = 0;
    uint32_t    _tempN = 0;

    bool        _tempFound = false;
    bool        _tempValid = false;
    int16_t     _tempValue = 0;
    uint32_t    _tempBadSince = 0;
    bool        _tempBadPending = false;
    bool        _menu = false;
    bool        _relayFaultLogged = false;

    uint32_t    _eventCount = 0;
    ErrorEvent  _events[EVENT_RING_SIZE] = {};
};
