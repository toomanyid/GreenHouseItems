#include "dev_Compat.h"
#include "dev_TempSensor.h"
#include "config.h"
#ifdef WINCTRL_UISIM
#include "dev_Sim.h"
#endif

void TempSensor::begin(uint8_t pin) {
    _pin = pin;
    analogSetPinAttenuation(_pin, ADC_11db);
    _first = true;
}

int32_t TempSensor::readUa() {
#ifdef WINCTRL_UISIM
    return SIM_TEMP_UA;         // UI 시험 빌드: 고정 온도
#endif
    int32_t mv = readMv(_pin, 4);
    int32_t ua = mv * 1000 / TEMP_SENSE_OHM;
    // 10ms마다 부르므로 가벼운 이동 평균 (약 0.08초)
    if (_first) { _avg = ua; _first = false; }
    else        _avg += (ua - _avg) / 8;
    return _avg;
}
