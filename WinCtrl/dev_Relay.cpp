#include "dev_Compat.h"
#include "dev_Relay.h"

static const int32_t DIVIDER_RATIO = 11;   // (100k + 10k) / 10k

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
    // 붙일 때 B 다음 A. 2층이 한 주기씩 나눠 켜므로 여기서는 그대로 건다
    digitalWrite(_b, fetB ? HIGH : LOW);
    digitalWrite(_a, fetA ? HIGH : LOW);
}

int32_t RelayDriver::relayMv() const { return readMv(_rs, 2) * DIVIDER_RATIO; }
int32_t RelayDriver::estopMv() const { return readMv(_es, 2) * DIVIDER_RATIO; }
