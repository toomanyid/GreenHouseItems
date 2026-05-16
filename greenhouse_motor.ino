// ============================================================
//  비닐하우스 개폐기 모터 제어기
//  Raspberry Pi Pico / Arduino IDE
//  Modbus RTU Slave / RS-485 / H-Bridge PWM
// ============================================================
#include "config.h"
#include "flash_storage.h"
#include "motor.h"
#include "current_sensor.h"
#include "modbus.h"
#include "oled.h"
#include "input.h"

// ============================================================
//  전역 상태
// ============================================================
Settings cfg;

enum class AppState {
    NORMAL,
    SETTINGS,
    AUTO_SETUP,
    OVERCURRENT_ERROR,
    BAUD_CHANGED
};

AppState appState = AppState::NORMAL;

// 설정 화면
int     settingIndex   = 0;
bool    editingValue   = false;
float   editValue      = 0.0f;
uint32_t lastInputMs   = 0;

// 자동 설정
int     autoStep       = 0;   // 0=bottom탐색, 1=top탐색
uint32_t autoStartMs   = 0;
uint32_t autoStepMs    = 0;

// 위치 변환
float   posSecToPercent(float sec, const Settings &s);
float   percentToPosSec(int pct, const Settings &s);

// ============================================================
//  위치 변환 함수
// ============================================================
float posSecToPercent(float sec, const Settings &s) {
    int32_t pos_ds = (int32_t)roundf(sec * 10.0f);
    if (pos_ds <= s.bottom_ds) return 0.0f;
    if (pos_ds <= s.zero_ds)   return 1.0f;
    int32_t range = s.top_ds - s.zero_ds;
    if (range <= 0) return 1.0f;
    float pct = 1.0f + (float)(pos_ds - s.zero_ds) / range * 98.0f;
    return constrain(pct, 1.0f, 100.0f);
}

float percentToPosSec(int pct, const Settings &s) {
    if (pct == 0) return s.bottom_ds / 10.0f;
    if (pct == 1) return s.zero_ds   / 10.0f;
    int32_t range = s.top_ds - s.zero_ds;
    return (s.zero_ds + (int32_t)((float)(pct - 1) / 99.0f * range)) / 10.0f;
}

// ============================================================
//  설정 항목 값 읽기/쓰기
// ============================================================
float getSettingValue(int idx) {
    switch (idx) {
        case 0: return (cfg.bottom_ds < 0 ? 0 : cfg.bottom_ds) / 10.0f;
        case 1: return (cfg.zero_ds   < 0 ? 0 : cfg.zero_ds)   / 10.0f;
        case 2: return (cfg.top_ds    < 0 ? 0 : cfg.top_ds)    / 10.0f;
        case 3: return (cfg.limit_ds  <= 0 ? cfg.top_ds : cfg.limit_ds) / 10.0f;
        case 4: return cfg.overcurrent_da / 10.0f;
        case 5: return (float)cfg.auto_timeout_s;
        case 6: return (float)cfg.slave_id;
        case 7: return (float)cfg.baud_code;
        default: return 0;
    }
}

void applySettingValue(int idx, float val) {
    switch (idx) {
        case 0: cfg.bottom_ds      = (int32_t)roundf(constrain(val, 0.0f, 9999.0f) * 10.0f); break;
        case 1: cfg.zero_ds        = (int32_t)roundf(constrain(val, 0.0f, 9999.0f) * 10.0f); break;
        case 2: cfg.top_ds         = (int32_t)roundf(constrain(val, 0.1f, 9999.0f) * 10.0f); break;
        case 3: {
            // limit은 0(=top과 동일) 또는 (0, top] 범위
            float top_f = cfg.top_ds / 10.0f;
            float lim   = constrain(val, 0.0f, top_f);
            cfg.limit_ds = (int32_t)roundf(lim * 10.0f);
            break;
        }
        case 4: cfg.overcurrent_da = (int32_t)roundf(constrain(val, 0.5f, 50.0f) * 10.0f); break;
        case 5: cfg.auto_timeout_s = (uint32_t)constrain(val, 10, 600); break;
        case 6: cfg.slave_id       = (uint8_t)constrain(val, 1, 247); break;
        case 7: cfg.baud_code      = (uint8_t)constrain(val, 0, BAUD_TABLE_SIZE - 1); break;
    }
}

float settingStep(int idx) {
    switch (idx) {
        case 0: case 1: case 2: case 3: return 0.5f;
        case 4: return 0.5f;
        case 5: return 10.0f;
        case 6: return 1.0f;
        case 7: return 1.0f;
        default: return 1.0f;
    }
}

// ============================================================
//  자동 설정 업데이트
// ============================================================
void updateAutoSetup(float current_a) {
    uint32_t now      = millis();
    float elapsed_sec = (now - autoStepMs) / 1000.0f;
    float total_sec   = (now - autoStartMs) / 1000.0f;

    oled.showAutoSetup(autoStep, elapsed_sec);

    // 타임아웃 체크
    if (total_sec > cfg.auto_timeout_s) {
        motor.emergencyStop();
        appState = AppState::NORMAL;
        return;
    }

    if (autoStep == 0) {
        // 1단계: 역방향 이동 → 전류 없을 때까지
        if (current_a < CURRENT_ZERO_THR && elapsed_sec > 1.0f) {
            // bottom 도달
            motor.emergencyStop();
            motor.setPosition(0.0f);
            cfg.bottom_ds = 0;
            cfg.zero_ds   = 0;

            // 2단계 시작
            autoStep   = 1;
            autoStepMs = millis();
            motor.moveTo(9999.0f, cfg.overcurrent_da / 10.0f);  // top방향으로 무한 이동
        }
    } else {
        // 2단계: 정방향 이동 → 전류 없을 때까지
        if (current_a < CURRENT_ZERO_THR && elapsed_sec > 1.0f) {
            // top 도달 — 모터가 추적한 실제 위치로 저장 (경과 시간은 duty 기반 위치와 불일치)
            motor.emergencyStop();
            cfg.top_ds   = (int32_t)roundf(motor.getPositionSec() * 10.0f);
            cfg.limit_ds = 0;  // top과 동일

            flashStorage.save(cfg);
            appState = AppState::NORMAL;
        }
    }
}

// ============================================================
//  일반 화면 업데이트
// ============================================================
void updateNormal() {
    float pos_sec = motor.getPositionSec();
    oled.showNormal(pos_sec);

    // 엔코더: 목표 위치 조정 (초 단위)
    int delta = input.getEncoderDelta();
    if (delta != 0 && flashStorage.isConfigured(cfg)) {
        float target = pos_sec + delta * 0.5f;
        target = constrain(target, cfg.bottom_ds / 10.0f,
                           flashStorage.effectiveLimit(cfg) / 10.0f);
        motor.moveTo(target, cfg.overcurrent_da / 10.0f);
    }

    // 택트 스위치: 스텝 이동
    if (input.isSwUpPressed() && flashStorage.isConfigured(cfg)) {
        float steps[] = { cfg.bottom_ds / 10.0f, cfg.zero_ds / 10.0f,
                          flashStorage.effectiveLimit(cfg) / 10.0f, cfg.top_ds / 10.0f };
        for (int i = 3; i >= 0; i--) {
            if (steps[i] > pos_sec + 0.1f) {
                motor.moveTo(steps[i], cfg.overcurrent_da / 10.0f);
                break;
            }
        }
    }
    if (input.isSwDownPressed() && flashStorage.isConfigured(cfg)) {
        float steps[] = { cfg.bottom_ds / 10.0f, cfg.zero_ds / 10.0f,
                          flashStorage.effectiveLimit(cfg) / 10.0f, cfg.top_ds / 10.0f };
        for (int i = 0; i < 4; i++) {
            if (steps[i] < pos_sec - 0.1f) {
                motor.moveTo(steps[i], cfg.overcurrent_da / 10.0f);
                break;
            }
        }
    }

    // 엔코더 길게 누름 → 설정 화면
    ButtonEvent ev = input.getEncSwEvent();
    if (ev == ButtonEvent::LONG_PRESS) {
        appState     = AppState::SETTINGS;
        settingIndex = 0;
        editingValue = false;
        lastInputMs  = millis();
        motor.stop();
    }

    // Modbus 목표 위치 처리
    if (modbus.hasNewTarget() && flashStorage.isConfigured(cfg)) {
        float target_sec = percentToPosSec(modbus.getTargetPct(), cfg);
        motor.moveTo(target_sec, cfg.overcurrent_da / 10.0f);
        modbus.clearNewTarget();
    }
}

// ============================================================
//  설정 화면 업데이트
// ============================================================
void updateSettings() {
    bool configured = flashStorage.isConfigured(cfg);

    // 10초 타임아웃 (설정 완료 상태에서만 적용)
    if (configured && (millis() - lastInputMs) > SETTINGS_TIMEOUT_MS) {
        appState = AppState::NORMAL;
        return;
    }

    // 상+하 동시 누름 → 탈출 (설정 완료 상태에서만)
    if (configured && input.isSwBothPressed()) {
        appState = AppState::NORMAL;
        return;
    }

    ButtonEvent ev = input.getEncSwEvent();
    int delta = input.getEncoderDelta();

    if (delta != 0) lastInputMs = millis();

    if (!editingValue) {
        // 항목 선택 모드
        if (input.isSwUpPressed()) {
            settingIndex = max(0, settingIndex - 1);
            lastInputMs  = millis();
        }
        if (input.isSwDownPressed()) {
            settingIndex = min(9, settingIndex + 1);
            lastInputMs  = millis();
        }
        if (ev == ButtonEvent::SHORT_PRESS) {
            lastInputMs = millis();
            if (settingIndex == 8) {
                // 자동 설정 시작
                appState   = AppState::AUTO_SETUP;
                autoStep   = 0;
                autoStartMs = autoStepMs = millis();
                motor.moveTo(-9999.0f, cfg.overcurrent_da / 10.0f);  // 역방향 무한 이동
                return;
            } else if (settingIndex == 9) {
                // 공장 초기화
                flashStorage.reset(cfg);
                flashStorage.save(cfg);
                appState = AppState::SETTINGS;
                return;
            } else {
                editingValue = true;
                editValue    = getSettingValue(settingIndex);
            }
        }
    } else {
        // 값 편집 모드
        if (delta != 0) {
            editValue += delta * settingStep(settingIndex);
            // 편집 중 실시간 범위 클램핑
            switch (settingIndex) {
                case 0: case 1: editValue = max(editValue, 0.0f); break;
                case 2: editValue = max(editValue, 0.1f); break;
                case 3: editValue = constrain(editValue, 0.0f, cfg.top_ds / 10.0f); break;
                case 4: editValue = constrain(editValue, 0.5f, 50.0f); break;
                case 5: editValue = constrain(editValue, 10.0f, 600.0f); break;
                case 6: editValue = constrain(editValue, 1.0f, 247.0f); break;
                case 7: editValue = constrain(editValue, 0.0f, (float)(BAUD_TABLE_SIZE - 1)); break;
            }
        }
        if (ev == ButtonEvent::SHORT_PRESS) {
            // 저장
            bool baudChanged = (settingIndex == 7 &&
                                (int)editValue != cfg.baud_code);
            applySettingValue(settingIndex, editValue);
            flashStorage.save(cfg);
            editingValue = false;
            lastInputMs  = millis();
            if (baudChanged) {
                appState = AppState::BAUD_CHANGED;
                return;
            }
        }
    }

    if (configured) {
        oled.showSettings(cfg, settingIndex, editValue, editingValue);
    } else {
        oled.showNotConfigured(settingIndex);
    }
}

// ============================================================
//  Modbus 상태 비트 조합
// ============================================================
uint16_t buildStatus() {
    uint16_t s = 0;
    if (motor.isMoving())                         s |= STATUS_MOVING;
    if (motor.getDir() == MotorDir::FORWARD)      s |= STATUS_DIR;
    if (motor.isError())                          s |= STATUS_OVERCUR;
    return s;
}

// ============================================================
//  setup / loop
// ============================================================
void setup() {
    // 설정 로드
    if (!flashStorage.load(cfg)) {
        flashStorage.reset(cfg);
    }

    // 하드웨어 초기화
    currentSensor.begin();
    motor.begin();
    oled.begin();
    input.begin();
    modbus.begin(cfg.slave_id, BAUD_TABLE[cfg.baud_code]);

    // 설정값 없으면 바로 설정 화면
    if (!flashStorage.isConfigured(cfg)) {
        appState     = AppState::SETTINGS;
        settingIndex = 0;
        editingValue = false;
        lastInputMs  = millis();
    }
}

void loop() {
    input.update();

    float current_a = currentSensor.readAmps();
    motor.update(current_a, cfg.overcurrent_da / 10.0f);

    // 과전류 에러 감지
    if (motor.isError() && appState != AppState::OVERCURRENT_ERROR) {
        appState = AppState::OVERCURRENT_ERROR;
    }

    // Modbus 업데이트
    float pos_pct = flashStorage.isConfigured(cfg)
                    ? posSecToPercent(motor.getPositionSec(), cfg)
                    : 1.0f;
    modbus.update(cfg, pos_pct, buildStatus(), current_a);

    // 화면/상태 업데이트
    switch (appState) {
        case AppState::NORMAL:
            updateNormal();
            break;

        case AppState::SETTINGS:
            updateSettings();
            break;

        case AppState::AUTO_SETUP:
            updateAutoSetup(current_a);
            break;

        case AppState::OVERCURRENT_ERROR:
            oled.showOvercurrentError(current_a);
            if (input.getEncSwEvent() == ButtonEvent::SHORT_PRESS) {
                motor.clearError();
                appState = AppState::NORMAL;
            }
            break;

        case AppState::BAUD_CHANGED:
            oled.showBaudChanged();
            break;
    }

    delay(10);
}
