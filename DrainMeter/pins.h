#pragma once
#include <Arduino.h>

// ============================================================
//  핀 배정 (Seeed XIAO ESP32C6)
//  자세한 배선은 README "핀 배치"와 "배선 메모"를 본다.
//
//  - 잠에서 깨울 수 있는 핀은 LP GPIO(0~7)뿐이다.
//    XIAO에서는 D0~D2(GPIO0~2)와 뒷면 패드 MTMS/MTDI/MTCK/MTDO(GPIO4~7).
//  - GPIO4(MTMS), GPIO5(MTDI)는 스트래핑 핀이라 쓰지 않는다.
//  - 외부 안테나 전환에 GPIO3, GPIO14를 보드가 쓴다 (헤더에 안 나옴).
// ============================================================

// ---- 깨우기 입력 (뒷면 패드, 외부 1MΩ 풀업) ----
// 부표 잡이 리드 스위치 2개를 직렬로: 둘 다 붙어 있으면 LOW, 하나라도 떨어지면 HIGH
const uint8_t PIN_LATCH_REED = 6;    // MTCK (GPIO6)
// 버튼: 누르면 LOW
const uint8_t PIN_BUTTON     = 7;    // MTDO (GPIO7)

// ---- 로드셀 (HX711 2개, 클럭 공용) ----
const uint8_t PIN_HX1_DOUT   = D0;   // 1라인 저울 데이터
const uint8_t PIN_HX2_DOUT   = D1;   // 2라인 저울 데이터
const uint8_t PIN_HX_SCK     = D2;   // 두 HX711 클럭 공용

// ---- I2C: RTC(DS3231), 나중에 PCF8574 확장 ----
const uint8_t PIN_I2C_SDA    = D4;
const uint8_t PIN_I2C_SCL    = D5;

// ---- 전자잉크 2.9인치 (SPI, 읽기선 MISO는 쓰지 않음) ----
const uint8_t PIN_EPD_CS     = D3;
const uint8_t PIN_EPD_DC     = D6;
const uint8_t PIN_EPD_RST    = D7;
const uint8_t PIN_EPD_SCK    = D8;
const uint8_t PIN_EPD_BUSY   = D9;
const uint8_t PIN_EPD_DIN    = D10;
