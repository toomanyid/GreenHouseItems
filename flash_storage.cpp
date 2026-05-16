#include "flash_storage.h"
#include "config.h"
#include <EEPROM.h>

#define EEPROM_ADDR 0

FlashStorage flashStorage;

FlashStorage::FlashStorage() {}

uint32_t FlashStorage::calcChecksum(const Settings &s) {
    uint32_t sum = 0;
    const uint8_t *p = (const uint8_t *)&s;
    // magic + 데이터 부분만 체크섬 (checksum 필드 자체는 제외)
    size_t len = sizeof(Settings) - sizeof(uint32_t);
    for (size_t i = 0; i < len; i++) {
        sum += p[i];
    }
    return sum;
}

bool FlashStorage::load(Settings &s) {
    EEPROM.begin(sizeof(Settings));
    EEPROM.get(EEPROM_ADDR, s);
    EEPROM.end();

    if (s.magic != SETTINGS_MAGIC) return false;
    if (s.checksum != calcChecksum(s)) return false;
    return true;
}

bool FlashStorage::save(const Settings &s) {
    Settings tmp = s;
    tmp.magic    = SETTINGS_MAGIC;
    tmp.checksum = calcChecksum(tmp);

    EEPROM.begin(sizeof(Settings));
    EEPROM.put(EEPROM_ADDR, tmp);
    bool ok = EEPROM.commit();
    EEPROM.end();
    return ok;
}

void FlashStorage::reset(Settings &s) {
    s.magic          = SETTINGS_MAGIC;
    s.bottom_sec     = -1.0f;   // 미설정 표시
    s.zero_sec       = -1.0f;
    s.top_sec        = -1.0f;
    s.limit_sec      = 0.0f;    // 0 = top과 동일
    s.overcurrent_a  = DEFAULT_OVERCURRENT_A;
    s.auto_timeout_s = DEFAULT_AUTO_TIMEOUT_S;
    s.slave_id       = DEFAULT_SLAVE_ID;
    s.baud_code      = DEFAULT_BAUD_CODE;
    s._pad[0]        = 0;
    s._pad[1]        = 0;
    s.checksum       = calcChecksum(s);
}

bool FlashStorage::isConfigured(const Settings &s) {
    // 필수값: bottom, zero, top 이 모두 0 이상이어야 함
    if (s.bottom_sec < 0.0f) return false;
    if (s.zero_sec   < 0.0f) return false;
    if (s.top_sec    <= 0.0f) return false;
    return true;
}

float FlashStorage::effectiveLimit(const Settings &s) {
    if (s.limit_sec <= 0.0f) return s.top_sec;
    return s.limit_sec;
}
