#include "dev_Compat.h"
#include "dev_Relay.h"
#if defined(WINCTRL_BENCH) || defined(WINCTRL_UISIM)
#include "dev_Sim.h"
#endif

void RelayDriver::begin(uint8_t pinFetA, uint8_t pinFetB, uint8_t pinRelaySense, uint8_t pinEstopSense) {
    _a = pinFetA; _b = pinFetB; _rs = pinRelaySense; _es = pinEstopSense;
    digitalWrite(_a, LOW);
    digitalWrite(_b, LOW);
    pinMode(_a, OUTPUT);
    pinMode(_b, OUTPUT);
    analogSetPinAttenuation(_rs, ADC_11db);
    analogSetPinAttenuation(_es, ADC_11db);
}

void RelayDriver::set(bool fetA, bool fetB) {
    _onA = fetA; _onB = fetB;
#ifdef WINCTRL_UISIM
    fetA = fetB = false;        // UI 시험 빌드: MC FET 핀은 늘 꺼 둔다
#endif
    // 붙일 때 B 다음 A. 2층이 한 주기씩 나눠 켜므로 여기서는 그대로 건다
    digitalWrite(_b, fetB ? HIGH : LOW);
    digitalWrite(_a, fetA ? HIGH : LOW);
}

#ifndef WINCTRL_UISIM
static const int32_t DIVIDER_RATIO = 11;   // (100k + 10k) / 10k
#endif

#if defined(WINCTRL_BENCH) || defined(WINCTRL_UISIM)
// 책상 시험, UI 시험 빌드: 24V가 없으므로 MC 뒤 전압을 읽지 않고
// 정상 MC처럼 FET A, B가 둘 다 켜졌을 때만 24V로 본다
int32_t RelayDriver::relayMv() const { return (_onA && _onB) ? SIM_24V_MV : 0; }
#else
int32_t RelayDriver::relayMv() const { return readMv(_rs, 2) * DIVIDER_RATIO; }
#endif

#ifdef WINCTRL_UISIM
int32_t RelayDriver::estopMv() const { return SIM_24V_MV; }   // UI 시험 빌드: 비상 버튼은 늘 정상
#else
int32_t RelayDriver::estopMv() const { return readMv(_es, 2) * DIVIDER_RATIO; }
#endif
