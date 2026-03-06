#ifndef MOTOR_POSITION_CONFIG_H
#define MOTOR_POSITION_CONFIG_H

#include <Arduino.h>

// 모터 위치 설정값 (동작시간 기준, 초 단위) + 전류 한계
// min: 하한, bottom: 기준(0% 표시), top: 동작 상한, max: 최대, maxCurrent: 전류 한계(A)
struct MotorPositionConfig {
  float min;        // 모터 하한
  float bottom;     // 기준 위치 (화면 0.0초)
  float top;        // 동작 상한 (이 위로 진행 불가)
  float max;        // 최대
  float maxCurrent; // 전류 한계 (A), 범위 1~10, 0.1 단위
  float target;     // Run 목표위치 (런타임, 저장 안 함)
  
  MotorPositionConfig()
    : min(0), bottom(5), top(10), max(15), maxCurrent(2.0f), target(0) {}
  
  float* getField(int index) {
    switch (index) {
      case 0: return &min;
      case 1: return &bottom;
      case 2: return &top;
      case 3: return &max;
      case 4: return &maxCurrent;
      default: return nullptr;
    }
  }
  
  static const char* getFieldName(int index) {
    switch (index) {
      case 0: return "min";
      case 1: return "bottom";
      case 2: return "top";
      case 3: return "max";
      case 4: return "maxCur";
      default: return "";
    }
  }
  
  static const int FIELD_COUNT = 5;
  
  // 위치(초) → 로터리용 % (bottom=0%, top=100%)
  static float positionToPercent(float posSec, float bottomSec, float topSec) {
    if (topSec <= bottomSec) return (posSec <= bottomSec) ? 0.0f : 100.0f;
    if (posSec <= bottomSec) return 0.0f;
    if (posSec >= topSec) return 100.0f;
    return (posSec - bottomSec) / (topSec - bottomSec) * 100.0f;
  }
};

// 싱글톤 + 플래시 저장/로드
class MotorPositionConfigManager {
public:
  static MotorPositionConfig& getInstance() {
    static MotorPositionConfig instance;
    return instance;
  }
  
  // 내부 플래시(NVS)에서 설정 로드, 없으면 기본값 유지
  static bool load();
  // 내부 플래시에 설정 저장
  static bool save();
};

#endif
