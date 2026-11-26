#include "dev_StatusLed.h"
#include "dev_Compat.h"

void StatusLed::begin(uint8_t pin) {
    _pin = pin;
    outputLow(pin);
    _on = false;
    _ready = true;
}

void StatusLed::set(bool on) {
    if (!_ready || on == _on) return;
    _on = on;
    digitalWrite(_pin, on ? HIGH : LOW);
}
