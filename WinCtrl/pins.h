#pragma once
#include <stdint.h>

// ============================================================
//  핀 배정 (Google 문서 "측창 모터 제어기 핀 배정 (ESP32-S3)" 기준)
//  핀 번호는 1층(dev_*)에서만 쓴다.
// ============================================================
const uint8_t PIN_ESTOP_SENSE  = 1;   // 비상정지 감지 (아날로그, 분압 100k/10k)
const uint8_t PIN_TEMP_SENSE   = 2;   // 온도 센서 4~20mA (감지 저항 120Ω)
const uint8_t PIN_M1_CURRENT   = 4;   // 모터1 전류 (R_IS + L_IS)
const uint8_t PIN_M2_CURRENT   = 5;   // 모터2 전류
const uint8_t PIN_RELAY_SENSE  = 6;   // 주 릴레이 뒤 전압 (분압 100k/10k)
const uint8_t PIN_S1           = 7;   // 원점 센서1 on/off (포토커플러, ON이면 LOW)
const uint8_t PIN_LCD_SDA      = 8;
const uint8_t PIN_LCD_SCL      = 9;
const uint8_t PIN_S2           = 10;  // 원점 센서2 on/off
const uint8_t PIN_M1_RPWM      = 11;  // 모터1 정방향(상승) PWM
const uint8_t PIN_M1_LPWM      = 12;  // 모터1 역방향(하강) PWM
const uint8_t PIN_M1_EN        = 13;  // 모터1 R_EN + L_EN
const uint8_t PIN_M2_RPWM      = 14;
const uint8_t PIN_M2_LPWM      = 15;
const uint8_t PIN_M2_EN        = 16;
const uint8_t PIN_RELAY_FET_A  = 17;  // 릴레이 코일 쪽 FET
// 18, 19, 20번은 여유 (19, 20은 직결 USB 핀)
const uint8_t PIN_RELAY_FET_B  = 21;  // GND 쪽 FET
const uint8_t PIN_MODEL_JUMPER = 39;  // 모델 구분: 열림(HIGH) 300W, GND(LOW) 50W
const uint8_t PIN_ENC_A        = 41;
const uint8_t PIN_ENC_B        = 42;
const uint8_t PIN_ENC_SW       = 47;

// 피에조와 내장 LED는 보드마다 38/48이 바뀐다. 받은 보드로 확인 후 고친다.
const uint8_t PIN_BUZZER       = 38;
const uint8_t PIN_STATUS_LED   = 48;
