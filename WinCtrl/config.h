#pragma once
#include <stdint.h>

// ============================================================
//  버전과 제품명
// ============================================================
const uint8_t  FW_VERSION_MAJOR = 0;
const uint8_t  FW_VERSION_MINOR = 9;
#define PRODUCT_NAME "WIN CTRL"            // 10글자까지 (임시)

// ============================================================
//  단위
//    시간과 위치: cs (0.01초 = 1), 타이머: ms, 전류: mA,
//    출력 비율: 천분율 (1000 = 100%), 온도: 0.1도 = 1
// ============================================================
typedef int32_t cs_t;
const cs_t     CS_EMPTY = -1;               // 설정값이 비어 있음
const cs_t     CS_STEP  = 50;               // 목표와 직접 입력값의 단위 (0.5초)

// ============================================================
//  제어 주기와 구동
// ============================================================
const uint32_t CONTROL_PERIOD_MS     = 10;
const uint32_t UI_PERIOD_MS          = 20;
const uint32_t SOFT_START_MS         = 600;
const uint32_t REVERSE_WAIT_MS       = 1000;  // 방향 전환 정지 시간 (전원 켠 뒤 첫 구동 전에도)
const uint32_t RELAY_SETTLE_MS       = 100;   // 주 차단 MC를 붙인 뒤 구동까지
const uint32_t RELAY_RELEASE_DELAY_MS = 100;  // 끊은 뒤 MC를 떼기까지 (임시)
const uint32_t RELAY_HOLD_MS         = 10000; // 두 창이 모두 멈춘 뒤 MC를 더 붙여 두는 시간 (스펙 6절)
const uint32_t PWM_FREQ_HZ           = 20000;
const uint16_t DUTY_FULL             = 1000;

// ============================================================
//  전류 판정
// ============================================================
const uint32_t OVERLOAD_WINDOW_MS    = 300;   // 과부하 판정 평균 시간
const uint32_t START_IGNORE_MS       = 1000;  // 기동 후 판정 제외 시간
const uint32_t OVERLOAD_BACKOFF_MS   = 1500;  // 과부하 후 되돌림 시간
const int32_t  NO_CURRENT_MA         = 300;   // 전류 없음으로 보는 경계
const int32_t  FIXED_LIMIT_MA_50W    = 5000;  // 모델별 고정 상한
const int32_t  FIXED_LIMIT_MA_300W   = 20000;
const uint32_t REFAULT_WINDOW_MS     = 60000; // 해제 후 이 시간 안에 다시 고장 나면 위치를 "모름"으로 (임시)

// 전류 환산: BTS7960 IS 전류 = 부하 전류 / KILIS. 실물로 확인 후 조정 (임시)
const int32_t  BTS_KILIS             = 8500;
const int32_t  SENSE_OHM_50W         = 2200;
const int32_t  SENSE_OHM_300W        = 820;
const int32_t  DRIVER_FAULT_MV       = 3000;  // 감지 전압이 이 이상이면 드라이버 고장 신호 (임시)

// ============================================================
//  원점 센서와 위치
// ============================================================
const uint32_t SENSOR_DEBOUNCE_MS    = 100;   // 같은 값이 이어져야 상태를 바꿈 (임시)
const uint32_t HOME_EXTRA_DOWN_MS    = 1000;  // 원점 잡기에서 더 내리는 시간
const cs_t     POS_CHECK_MARGIN      = 500;   // 센서 지점보다 5초 넘게 위인데 "있음"이면 이상 (임시)
const cs_t     CLOSE_MARGIN          = 300;   // CLOSE에서 겹침 하강 시간에 더하는 여유 (임시)
const cs_t     LIMIT_BACKUP_MARGIN   = 50;    // 상한 리밋 세 번째 겹의 여유 (임시)
const uint32_t MAX_RUN_FALLBACK_MS   = 1200000; // 최대 구동 시간을 알 수 없을 때 (20분)

// ============================================================
//  이동 시간 측정
// ============================================================
const uint32_t CAL_TIMEOUT_MS        = 600000; // 10분

// ============================================================
//  릴레이와 비상정지 (분압 100k/10k: 24V → 약 2.18V)
// ============================================================
const int32_t  RELAY_ON_MV           = 12000; // 릴레이 뒤 24V 쪽 전압이 이 이상이면 "있음" (임시)
const int32_t  RELAY_OFF_MV          = 5000;  // 이 미만이면 "없음" (임시)
const uint32_t RELAY_DECAY_MS        = 3000;  // 뗀 뒤 전압이 빠지기를 기다리는 시간 (임시)
const uint32_t RELAY_SELFTEST_STEP_MS = 200;
const int32_t  ESTOP_OK_MV           = 12000; // 비상 버튼 뒤 전압이 이 미만이면 비상정지 (임시)
const uint32_t ESTOP_DEBOUNCE_MS     = 30;

// ============================================================
//  조작과 화면
// ============================================================
const uint32_t INPUT_COMMIT_MS       = 2000;  // 입력 확정 시간
const uint32_t LONG_PRESS_MS         = 3000;
const uint32_t SPLASH_MS             = 3000;
const uint32_t BLINK_MS              = 500;
const uint32_t ALT_TEXT_MS           = 1000;  // 상태 문구와 초 표시를 번갈아 보이는 간격
const uint32_t BACKLIGHT_OFF_MS      = 300000;
const uint32_t MENU_TIMEOUT_MS       = 120000; // 설정 메뉴 자동 나가기
const uint32_t ENC_VALID_WINDOW_MS   = 500;   // 이 안에 같은 방향 두 칸이어야 입력으로 받음
const uint32_t ENC_FAST_MS           = 80;    // 칸 사이가 이보다 짧으면 5초 단위 (임시)
const uint32_t ENC_FASTER_MS         = 40;    // 이보다 짧으면 10초 단위 (임시)
const uint8_t  ENC_EDGES_PER_DETENT  = 4;     // PEC16-4220F-S0024: 한 칸에 4번 바뀜
const int8_t   ENC_DIRECTION         = -1;    // 시계 방향이 +가 되게. 반대로 돌면 1로 바꾼다 (S1/S2 모듈로 맞춤, PEC16 오면 다시 확인)
const uint32_t LCD_REINIT_MS         = 60000; // 화면을 주기적으로 다시 초기화

// ============================================================
//  온도 센서
// ============================================================
const uint32_t TEMP_DETECT_MS        = 2000;
const int32_t  TEMP_SENSE_OHM        = 150;
const int32_t  TEMP_PRESENT_UA       = 2000;  // 2mA 이상이면 "있음" (임시)
const int32_t  TEMP_OVER_UA          = 21000; // 21mA를 넘으면 이상 (임시)
const uint32_t TEMP_LOSS_MS          = 1000;  // 끊김이 이 시간 이어지면 끊김으로 봄
const uint32_t TEMP_LOG_MS           = 10000; // 온도 기록 간격
const uint16_t TEMP_LOG_SIZE         = 180;   // 기울기 구간 최대 30분 / 10초

// ============================================================
//  워치독, 에러 저장
// ============================================================
const uint32_t WATCHDOG_TIMEOUT_S    = 2;     // (임시)
const uint8_t  ERROR_LOG_SIZE        = 16;
