#pragma once
#include <stdint.h>
#include <stddef.h>
struct Preferences {
    bool begin(const char*, bool) { return true; }
    size_t getBytesLength(const char*) { return 0; }
    size_t getBytes(const char*, void*, size_t) { return 0; }
    size_t putBytes(const char*, const void*, size_t n) { return n; }
    uint8_t getUChar(const char*, uint8_t d) { return d; }
    size_t putUChar(const char*, uint8_t) { return 1; }
    uint16_t getUShort(const char*, uint16_t d) { return d; }
    size_t putUShort(const char*, uint16_t) { return 2; }
};
