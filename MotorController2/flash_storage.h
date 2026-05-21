#pragma once
#include <Arduino.h>

// ============================================================
//  설정값 구조체
// ============================================================
struct Settings {
    // 헤더 (유효성 확인용)
    uint32_t magic;             // 0xDEADBEEF 이면 유효

    // 필수 설정값 (단위: 0.1초 = decisecond, 10배수)
    int32_t  bottom_ds;         // bottom 위치 (예: 125 = 12.5초)
    int32_t  zero_ds;           // zero 위치
    int32_t  top_ds;            // top 위치

    // 선택 설정값
    int32_t  limit_ds;          // limit 위치 (0이면 top과 동일)
    int32_t  overcurrent_da;    // 과전류 임계값 (단위: 0.1A, 예: 20 = 2.0A)
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

    // limit_ds 실효값 반환 (0이면 top_ds), 단위: ds
    int32_t effectiveLimit(const Settings &s);

private:
    uint32_t calcChecksum(const Settings &s);
};

extern FlashStorage flashStorage;
