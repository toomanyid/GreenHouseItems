#include "ErrorLog.h"

void ErrorLog::removeAt(uint8_t i) {
    for (uint8_t k = i + 1; k < _count; k++) _e[k - 1] = _e[k];
    _count--;
}

bool ErrorLog::add(uint8_t window, Cause cause, uint16_t bootCount, uint32_t uptimeS) {
    if (hasUnacked(window, cause)) return false;
    if (_count >= ERROR_LOG_SIZE) {
        // 가득 차면 확인한 것 중 가장 오래된 것부터 지운다. 전부 미확인이면 가장 오래된 것
        uint8_t victim = 0;
        for (uint8_t i = 0; i < _count; i++) {
            if (_e[i].acked) { victim = i; break; }
        }
        removeAt(victim);
    }
    ErrorEntry& e = _e[_count++];
    e.window = window;
    e.cause = cause;
    e.acked = false;
    e.bootCount = bootCount;
    e.uptimeS = uptimeS;
    return true;
}

bool ErrorLog::ackOldest(ErrorEntry& acked) {
    for (uint8_t i = 0; i < _count; i++) {
        if (!_e[i].acked) {
            _e[i].acked = true;
            acked = _e[i];
            return true;
        }
    }
    return false;
}

bool ErrorLog::hasUnacked() const {
    for (uint8_t i = 0; i < _count; i++) if (!_e[i].acked) return true;
    return false;
}

bool ErrorLog::hasUnacked(uint8_t window, Cause cause) const {
    for (uint8_t i = 0; i < _count; i++) {
        if (!_e[i].acked && _e[i].window == window && _e[i].cause == cause) return true;
    }
    return false;
}

bool ErrorLog::hasAny(uint8_t window, Cause cause) const {
    for (uint8_t i = 0; i < _count; i++) {
        if (_e[i].window == window && _e[i].cause == cause) return true;
    }
    return false;
}
