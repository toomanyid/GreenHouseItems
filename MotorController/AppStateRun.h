#ifndef APP_STATE_RUN_H
#define APP_STATE_RUN_H

#include "AppState.h"

#define RUN_ARRIVAL_THRESHOLD 0.05f  // 목표 도달 판정 (초)
#define RUN_ENCODER_STEP 0.1f    // 엔코더 1클릭당 목표 변경량 (초)

// Run 상태: Idle에서 2초 대기 후 진입, 모터 동작 → 도달 시 Idle
// 로터리 회전 시 모터 멈춤, 목표 갱신, Idle 복귀
class AppStateRun : public AppState {
private:
  float targetPositionSec;
  int lastEncoderValue;
  
  AppStateRun();
  AppStateRun(const AppStateRun&) = delete;
  void operator=(const AppStateRun&) = delete;

public:
  void onEnter() override;
  void onUpdate() override;
  const char* getName() const override { return "Run"; }
  
  static AppStateRun& getInstance();
};

#endif
