#pragma once
#include <Arduino.h>
#include "config.h"
#include "flash_storage.h"

class ModbusRTU {
public:
    ModbusRTU();
    void begin(uint8_t slave_id, uint32_t baud);
    void update(Settings &cfg, float current_pos_pct, uint16_t status, float current_a);

    // 마지막으로 수신된 목표 위치 % (0~100)
    int  getTargetPct() const { return _target_pct; }
    bool hasNewTarget() const { return _new_target; }
    void clearNewTarget()     { _new_target = false; }

private:
    uint8_t  _slave_id;
    int      _target_pct;
    bool     _new_target;

    uint8_t  _buf[256];
    uint8_t  _buf_len;
    uint32_t _last_rx_us;

    uint16_t crc16(const uint8_t *data, uint8_t len);
    void     sendResponse(const uint8_t *data, uint8_t len);
    void     processFrame(uint8_t *frame, uint8_t len, Settings &cfg,
                          float pos_pct, uint16_t status, float current_a);

    // 응답 함수
    void     respondFC03(uint8_t *frame, Settings &cfg);
    void     respondFC04(uint8_t *frame, float pos_pct, uint16_t status, float current_a);
    void     respondFC06(uint8_t *frame, Settings &cfg, bool broadcast);
    void     respondFC16(uint8_t *frame, Settings &cfg, bool broadcast);
    void     respondException(uint8_t fc, uint8_t code);
};

extern ModbusRTU modbus;
