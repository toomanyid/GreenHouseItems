#pragma once
// ============================================================
//  1층: 아두이노 ESP32 코어 2.x와 3.x의 차이를 여기서만 감싼다.
//  (LEDC PWM 함수 이름이 3.0에서 바뀌었다)
// ============================================================
#include <Arduino.h>

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define WINCTRL_CORE3 1
#else
  #define WINCTRL_CORE3 0
#endif

// PWM 핀 하나를 준비한다. channel은 2.x에서만 쓴다 (3.x는 핀으로 다룸)
inline void pwmAttach(uint8_t pin, uint8_t channel, uint32_t freq, uint8_t bits) {
#if WINCTRL_CORE3
    (void)channel;
    ledcAttach(pin, freq, bits);
#else
    ledcSetup(channel, freq, bits);
    ledcAttachPin(pin, channel);
#endif
}

inline void pwmWrite(uint8_t pin, uint8_t channel, uint32_t duty) {
#if WINCTRL_CORE3
    (void)channel;
    ledcWrite(pin, duty);
#else
    (void)pin;
    ledcWrite(channel, duty);
#endif
}

inline void pwmTone(uint8_t pin, uint8_t channel, uint32_t freq) {
#if WINCTRL_CORE3
    (void)channel;
    ledcWriteTone(pin, freq);
#else
    (void)pin;
    ledcWriteTone(channel, freq);
#endif
}

// 아날로그 핀을 11dB(약 0~3.1V)로 준비한다.
// 3.x는 한 번 읽어 ADC 채널로 잡힌 핀에만 감쇠를 걸 수 있다
// (먼저 걸면 "Pin is not configured as analog channel" 에러가 나고 무시된다)
inline void adcInit(uint8_t pin) {
    (void)analogReadMilliVolts(pin);
    analogSetPinAttenuation(pin, ADC_11db);
}

// 출력 핀을 LOW로 준비한다.
// 3.x는 pinMode 전에 digitalWrite를 하면 에러를 내고 무시하므로 pinMode를 먼저 한다.
// 리셋 직후 출력 레지스터는 0이라 OUTPUT으로 바뀌는 순간에도 LOW다
inline void outputLow(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

// 아날로그 핀을 mV로 여러 번 읽어 평균
inline int32_t readMv(uint8_t pin, uint8_t samples) {
    int32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) sum += (int32_t)analogReadMilliVolts(pin);
    return sum / samples;
}

// LEDC 채널 (2.x용). 모터 4개, 피에조 1개
const uint8_t LEDC_CH_M1_R = 0;
const uint8_t LEDC_CH_M1_L = 1;
const uint8_t LEDC_CH_M2_R = 2;
const uint8_t LEDC_CH_M2_L = 3;
const uint8_t LEDC_CH_BUZZER = 4;
const uint8_t PWM_BITS = 10;          // 20kHz에서 10비트 (0~1023)
