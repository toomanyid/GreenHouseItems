#include "RotaryEncoder.h"

RotaryEncoder* RotaryEncoder::instance = nullptr;

void IRAM_ATTR RotaryEncoder::isr() {
  if (instance != nullptr) {
    instance->handleInterrupt();
  }
}

void RotaryEncoder::handleInterrupt() {
  int clkState = digitalRead(clkPin);
  int dtState = digitalRead(dtPin);
  
  int currentState = (clkState << 1) | dtState;
  
  // 상태 변화 감지 - 상태가 00일 때만 카운트
  if (lastState != currentState) {
    if (lastState != 0 && currentState == 0) {
      if (lastState == 1) {
        value++;
      } else if (lastState == 2) {
        value--;
      }
    }
    
    lastState = currentState;
  }
}

RotaryEncoder::RotaryEncoder() : clkPin(0), dtPin(0), value(0), lastState(0) {
}

void RotaryEncoder::init(int clk, int dt) {
  clkPin = clk;
  dtPin = dt;
  value = 0;
  instance = this;
  
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin, INPUT_PULLUP);
  
  int clkState = digitalRead(clkPin);
  int dtState = digitalRead(dtPin);
  lastState = (clkState << 1) | dtState;
  
  attachInterrupt(digitalPinToInterrupt(clkPin), RotaryEncoder::isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(dtPin), RotaryEncoder::isr, CHANGE);
}

int RotaryEncoder::getValue() {
  noInterrupts();
  int val = value;
  interrupts();
  return val;
}

void RotaryEncoder::reset() {
  noInterrupts();
  value = 0;
  interrupts();
}
