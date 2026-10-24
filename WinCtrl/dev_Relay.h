#pragma once
#include <stdint.h>

// ============================================================
//  1층: 주 차단 MC FET 두 개와 24V 쪽 전압 감지.
//  MC 뒤 전압과 비상 버튼 뒤 전압은 둘 다 100k/10k 분압으로 읽는다.
//  붙이는 순서와 자가 점검 판정은 2층 RelayControl에 있다.
// ============================================================
class RelayDriver {
public:
    void    begin(uint8_t pinFetA, uint8_t pinFetB, uint8_t pinRelaySense, uint8_t pinEstopSense);
    void    set(bool fetA, bool fetB);
    int32_t relayMv() const;     // MC 뒤 24V 쪽 전압 (mV)
    int32_t estopMv() const;     // 비상 버튼 뒤 24V 쪽 전압 (mV)
private:
    uint8_t _a = 0, _b = 0, _rs = 0, _es = 0;
};
