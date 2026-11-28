#pragma once
#include <stdint.h>
#include <stddef.h>
// 문법 확인용 가짜 LittleFS
struct File {
    explicit operator bool() const { return false; }
    size_t read(uint8_t*, size_t) { return 0; }
    size_t write(const uint8_t*, size_t) { return 0; }
    bool   seek(uint32_t) { return true; }
    size_t size() { return 0; }
    void   close() {}
};
struct LittleFSFS {
    bool   begin(bool = false) { return true; }
    File   open(const char*, const char* = "r") { return File(); }
    bool   exists(const char*) { return false; }
    size_t totalBytes() { return 0; }
    size_t usedBytes() { return 0; }
};
static LittleFSFS LittleFS;
