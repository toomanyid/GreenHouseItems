#ifndef MOTOR_POSITION_CONFIG_H
#define MOTOR_POSITION_CONFIG_H

#include <Arduino.h>

// 모터 위치 설정값: 시간은 10배수 정수 보관 (예: 12.5초 → 125), 전류는 float
// min: 하한, bottom: 기준(0% 표시), top: 동작 상한, max: 최대, maxCurrent: 전류 한계(A)
struct MotorPositionConfig {
  int min_x10;      // 모터 하한 (초×10)
  int bottom_x10;   // 기준 위치 (화면 0.0초)
  int top_x10;      // 동작 상한 (이 위로 진행 불가)
  int max_x10;      // 최대
  float maxCurrent; // 전류 한계 (A), 범위 1~10, 0.1 단위
  int target_x10;   // Run 목표위치 (런타임, 저장 안 함)
  
  MotorPositionConfig()
    : min_x10(0), bottom_x10(50), top_x10(100), max_x10(150), maxCurrent(2.0f), target_x10(0) {}
  
  // 시간 필드(0~3)용: 10배수 정수 포인터
  int* getTimeField_x10(int index) {
    switch (index) {
      case 0: return &min_x10;
      case 1: return &bottom_x10;
      case 2: return &top_x10;
      case 3: return &max_x10;
      default: return nullptr;
    }
  }
  
  // 전류 필드(4)용
  float* getMaxCurrentPtr() { return &maxCurrent; }
  
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
  
  float getMinSec() const { return min_x10 / 10.0f; }
  float getBottomSec() const { return bottom_x10 / 10.0f; }
  float getTopSec() const { return top_x10 / 10.0f; }
  float getMaxSec() const { return max_x10 / 10.0f; }
  float getTargetSec() const { return target_x10 / 10.0f; }
  void setTargetSec(float sec) { target_x10 = (int)(sec * 10.0f); }
  
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
