#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Types.h"

// ============================================================
//  에러 저장 (스펙 11절). 16건까지, 오래된 것부터 순서대로 담는다.
//  플래시에 쓰고 읽는 것은 Store에서 한다.
// ============================================================

struct ErrorEntry {
    uint8_t  window;      // 1, 2, 0 = 공통
    Cause    cause;
    bool     acked;
    uint16_t bootCount;   // 전원을 켠 횟수
    uint32_t uptimeS;     // 전원을 켠 뒤 지난 시간
};

class ErrorLog {
public:
    void clear() { _count = 0; }

    // 같은 창의 같은 원인이 미확인으로 이미 있으면 추가하지 않는다. 추가했으면 true
    bool add(uint8_t window, Cause cause, uint16_t bootCount, uint32_t uptimeS);

    // 가장 오래된 미확인 에러 하나를 확인 처리. 없으면 false
    bool ackOldest(ErrorEntry& acked);

    bool    hasUnacked() const;
    bool    hasUnacked(uint8_t window, Cause cause) const;
    bool    hasAny(uint8_t window, Cause cause) const;   // 확인한 것 포함
    uint8_t count() const { return _count; }
    const ErrorEntry& at(uint8_t i) const { return _e[i]; }           // 0 = 가장 오래된 것
    const ErrorEntry& newest(uint8_t i) const { return _e[_count - 1 - i]; }  // 0 = 가장 최근

    // 플래시 저장용
    uint8_t*       raw()           { return reinterpret_cast<uint8_t*>(_e); }
    static size_t  rawSize()       { return sizeof(ErrorEntry) * ERROR_LOG_SIZE; }
    void           setCount(uint8_t n) { _count = n > ERROR_LOG_SIZE ? ERROR_LOG_SIZE : n; }

private:
    void removeAt(uint8_t i);
    ErrorEntry _e[ERROR_LOG_SIZE] = {};
    uint8_t    _count = 0;
};
