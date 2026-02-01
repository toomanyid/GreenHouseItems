#include "AppStateIdle.h"
#include "AppState.h"
#include "AppStateSetup.h"
#include "MotorDriver.h"
#include "CurrentSensor.h"
#include "Button.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// 외부 참조 (MotorController.ino에서 정의)
extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern CurrentSensor& currentSensor;
extern Button button_push;
extern Button button_cw;
extern Button button_ccw;
extern AppStateContext appStateContext;
extern int lastEncoderValue;

AppStateIdle& AppStateIdle::getInstance() {
  static AppStateIdle instance;
  return instance;
}

void AppStateIdle::onEnter() {
  motor.stop();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
}

void AppStateIdle::onUpdate() {
  // 버튼 읽기
  button_push.update();
  button_cw.update();
  button_ccw.update();
  
  int currentValue = encoder.getValue();
  
  // PUSH: Setup 상태로 전환
  if (button_push.isPressed()) {
    appStateContext.setState(&AppStateSetup::getInstance());
    return;
  }
  
  // 버튼에 따른 모터 제어
  if (button_cw.isPressed()) {
    motor.runCW(100);
  } else if (button_ccw.isPressed()) {
    motor.runCCW(50);
  } else {
    motor.stop();
  }
  
  
  if (currentValue != lastEncoderValue) {
    lastEncoderValue = currentValue;
    Serial.print("엔코더 값: ");
    Serial.println(currentValue);
  }
  
  float current = currentSensor.getCurrent();
  
  // 디스플레이
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("= Motor controller  =");
  display.println("");
  display.print("State: ");
  display.println(getName());
  display.print("Time: ");
  display.print(millis() / 1000);
  display.println("Sec");
  display.print("Encoder: ");
  display.println(currentValue);
  display.print("Motor: ");
  if (motor.isRunning()) {
    display.print("RUN ");
    display.print(motor.getActualSpeed());
    display.print("%");
  } else {
    display.print("STOP");
  }
  display.println("");
  display.print("Pos: ");
  display.print(motor.getPositionSec(), 1);
  display.println("s");
  display.print("Current: ");
  display.print(current, 2);
  display.println("A");
  display.print("PUSH:");
  display.print(button_push.isPressed() ? "o" : "x");
  display.print(" CW:");
  display.print(button_cw.isPressed() ? "o" : "x");
  display.print(" CCW:");
  display.println(button_ccw.isPressed() ? "o" : "x");
  display.display();
}
