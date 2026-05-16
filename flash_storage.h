#pragma once
#include <Arduino.h>

// ============================================================
//  설정값 구조체
// ============================================================
struct Settings {
    // 헤더 (유효성 확인용)
    uint32_t magic;             // 0xDEADBEEF 이면 유효

    // 필수 설정값
    float    bottom_sec;        // bottom 위치 (초)
    float    zero_sec;          // zero 위치 (초)
    float    top_sec;           // top 위치 (초)

    // 선택 설정값
    float    limit_sec;         // limit 위치 (초), 0이면 top과 동일
    float    overcurrent_a;     // 과전류 임계값 (A)
    uint32_t auto_timeout_s;    // 자동설정 최대시간 (초)
    uint8_t  slave_id;          // Modbus 슬레이브 ID
    uint8_t  baud_code;         // 보레이트 코드 0~4
    uint8_t  _pad[2];           // 4바이트 정렬 패딩

    uint32_t checksum;          // 단순 체크섬
};

const uint32_t SETTINGS_MAGIC = 0xDEADBEEF;

class FlashStorage {
public:
    FlashStorage();

    // 설정 로드. 유효하지 않으면 false 반환
    bool load(Settings &s);

    // 설정 저장
    bool save(const Settings &s);

    // 설정 초기화 (공장 초기화)
    void reset(Settings &s);

    // 필수값이 모두 설정되었는지 확인
    bool isConfigured(const Settings &s);

    // limit_sec 실효값 반환 (0이면 top_sec)
    float effectiveLimit(const Settings &s);

private:
    uint32_t calcChecksum(const Settings &s);
};

extern FlashStorage flashStorage;
