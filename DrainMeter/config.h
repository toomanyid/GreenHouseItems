#pragma once

// ============================================================
//  상수와 실물로 맞춰야 하는 값
//  "실물 확인" 표시가 있는 값은 부품 시험으로 구해서 고친다 (README 참고).
// ============================================================

#define DRAINMETER_VERSION "0.9-dev"

// ---- 로드셀 ----
// 저울 값 1g당 HX711 읽음값. test_scale의 보정 명령으로 구한다. [실물 확인]
const float SCALE1_COUNTS_PER_G = 1.0f;
const float SCALE2_COUNTS_PER_G = 1.0f;
// HX711이 깨어난 뒤 값이 안정될 때까지 버리는 시간 (10회/초 설정 기준 약 400ms)
const uint32_t HX_SETTLE_MS = 450;
// 두 HX711이 준비되기를 기다리는 최대 시간
const uint32_t HX_READY_TIMEOUT_MS = 500;

// ---- RTC ----
const uint8_t I2C_ADDR_DS3231  = 0x68;
const uint8_t I2C_ADDR_PCF8574 = 0x20;   // 1.0에서 추가 예정

// ---- 전자잉크 ----
// 패널 종류. 화면이 깨지거나 아무것도 안 나오면 README "전자잉크 패널 종류" 표를 보고 바꾼다. [실물 확인]
#define EPD_DRIVER GxEPD2_290_T94_V2
// 0°C 아래에서는 갱신하지 않는다 (패널 사용 온도 밖)
const int EPD_MIN_TEMP_C = 0;
