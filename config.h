#pragma once
#include <Arduino.h>

// ============================================================
//  핀 정의 (와이어링 시 여기서 수정)
// ============================================================
const uint8_t PIN_UART_TX   = 0;   // MAX485 DI
const uint8_t PIN_UART_RX   = 1;   // MAX485 RO
const uint8_t PIN_DE_RE     = 2;   // MAX485 DE/RE
const uint8_t PIN_I2C_SDA   = 4;   // OLED SDA
const uint8_t PIN_I2C_SCL   = 5;   // OLED SCL
const uint8_t PIN_ENC_A     = 6;   // 엔코더 A
const uint8_t PIN_ENC_B     = 7;   // 엔코더 B
const uint8_t PIN_ENC_SW    = 8;   // 엔코더 푸시
const uint8_t PIN_SW_UP     = 9;   // 택트 스위치 상
const uint8_t PIN_PWM_A     = 10;  // H-Bridge 정회전
const uint8_t PIN_PWM_B     = 11;  // H-Bridge 역회전
const uint8_t PIN_MOTOR_EN  = 12;  // H-Bridge Enable
const uint8_t PIN_SW_DOWN   = 13;  // 택트 스위치 하
const uint8_t PIN_ACS758    = 26;  // 전류 센서 ADC

// ============================================================
//  모터 제어 상수
// ============================================================
const uint32_t PWM_FREQ_HZ      = 20000;   // 20kHz
const uint32_t RAMP_TIME_MS     = 1000;    // 소프트 스타트/스톱 1초
const uint32_t RAMP_STEPS       = 100;     // 램프 스텝 수
const uint32_t RAMP_STEP_MS     = RAMP_TIME_MS / RAMP_STEPS;

// ============================================================
//  전류 센서 상수 (ACS758 50A)
// ============================================================
const float    ACS758_VREF      = 2.5f;    // 무전류 기준전압 (V)
const float    ACS758_SENS      = 0.04f;   // 감도 40mV/A
const float    ADC_VREF         = 3.3f;    // Pico ADC 기준전압
const int      ADC_RESOLUTION   = 4096;    // 12bit
const float    CURRENT_ZERO_THR = 0.5f;    // 전류 없음 판단 임계 (A)

// ============================================================
//  Modbus 기본값
// ============================================================
const uint8_t  MODBUS_DEFAULT_ID   = 1;
const uint32_t MODBUS_DEFAULT_BAUD = 9600;

// 보레이트 코드 → 실제 baud 변환 테이블
const uint32_t BAUD_TABLE[] = {9600, 19200, 38400, 57600, 115200};
const uint8_t  BAUD_TABLE_SIZE = 5;

// Modbus Holding Register 주소
const uint16_t HR_TARGET_POS  = 0x0000;  // 목표 위치 0~100%
const uint16_t HR_SLAVE_ID    = 0x0001;  // 슬레이브 ID
const uint16_t HR_BAUD_CODE   = 0x0002;  // 보레이트 코드

// Modbus Input Register 주소
const uint16_t IR_CURRENT_POS = 0x0000;  // 현재 위치 0~100%
const uint16_t IR_STATUS      = 0x0001;  // 상태 비트필드
const uint16_t IR_CURRENT_MA  = 0x0002;  // 전류값 (0.1A 단위)

// 상태 비트
const uint16_t STATUS_MOVING   = (1 << 0);
const uint16_t STATUS_DIR      = (1 << 1);  // 1=정회전
const uint16_t STATUS_OVERCUR  = (1 << 2);
const uint16_t STATUS_LIMIT    = (1 << 3);

// ============================================================
//  설정값 기본값
// ============================================================
const float    DEFAULT_OVERCURRENT_A  = 2.0f;   // 과전류 임계 2A
const uint32_t DEFAULT_AUTO_TIMEOUT_S = 300;    // 자동설정 최대 5분
const uint8_t  DEFAULT_BAUD_CODE      = 0;      // 9600bps
const uint8_t  DEFAULT_SLAVE_ID       = 1;

// ============================================================
//  UI 상수
// ============================================================
const uint32_t SETTINGS_TIMEOUT_MS  = 10000;  // 설정 자동 탈출 10초
const uint32_t LONG_PRESS_MS        = 3000;   // 길게 누름 3초
const uint32_t DEBOUNCE_MS          = 20;     // 디바운스
const uint32_t ENCODER_POLL_MS      = 5;      // 엔코더 폴링 주기

// ============================================================
//  위치 상수
// ============================================================
const float    POS_STEP_BOTTOM = 0.0f;   // 내부 타이머 기준
