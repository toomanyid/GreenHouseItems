#pragma once
#include <stdint.h>
#include <stddef.h>
struct TwoWire {
    bool begin(int, int, uint32_t) { return true; }
    void setTimeOut(uint16_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission() { return 0; }
    size_t write(uint8_t) { return 1; }
    void end() {}
};
extern TwoWire Wire;
