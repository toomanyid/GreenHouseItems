#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>

class RotaryEncoder {
private:
  int clkPin;
  int dtPin;
  volatile int value;
  volatile int lastState;
  
  static RotaryEncoder* instance;
  static void IRAM_ATTR isr();
  
  void handleInterrupt();
  
public:
  RotaryEncoder();
  
  void init(int clkPin, int dtPin);
  int getValue();
  void reset();
};

#endif
