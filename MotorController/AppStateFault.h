#ifndef APP_STATE_FAULT_H
#define APP_STATE_FAULT_H

#include "AppState.h"

// Fault 상태: 전류 초과 등 오류, PUSH로 Idle 복귀
class AppStateFault : public AppState {
public:
  void onEnter() override;
  void onUpdate() override;
  void onKey(Key key) override;
  const char* getName() const override { return "Fault"; }

  static AppStateFault& getInstance();

private:
  AppStateFault() = default;
  AppStateFault(const AppStateFault&) = delete;
  void operator=(const AppStateFault&) = delete;
};

#endif
