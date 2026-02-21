#include "AppStateFault.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "MotorDriver.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;
extern MotorDriver& motor;
extern AppStateContext appStateContext;

AppStateFault& AppStateFault::getInstance() {
  static AppStateFault instance;
  return instance;
}

void AppStateFault::onEnter() {
  motor.stop();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
}

void AppStateFault::onKey(Key key) {
  if (key == Key::Push) {
    appStateContext.setState(&AppStateIdle::getInstance());
  }
}

void AppStateFault::onUpdate() {
  // 디스플레이: Fault 표시
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("FAULT");
  display.setTextSize(1);
  display.setCursor(0, 36);
  display.println("Over current");
  display.setCursor(0, 48);
  display.println("PUSH -> Idle");
  display.display();
}
