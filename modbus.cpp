#include "modbus.h"

ModbusRTU modbus;

// 3.5 문자 시간 (us) = 3.5 * 11 * 1e6 / baud
#define INTER_FRAME_US(baud) (uint32_t)(38500000UL / (baud))

ModbusRTU::ModbusRTU()
    : _slave_id(1), _target_pct(1), _new_target(false)
    , _buf_len(0), _last_rx_us(0)
{}

void ModbusRTU::begin(uint8_t slave_id, uint32_t baud) {
    _slave_id = slave_id;
    Serial1.begin(baud, SERIAL_8N1);
    pinMode(PIN_DE_RE, OUTPUT);
    digitalWrite(PIN_DE_RE, LOW);  // 수신 모드
}

uint16_t ModbusRTU::crc16(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return crc;
}

void ModbusRTU::sendResponse(const uint8_t *data, uint8_t len) {
    uint16_t crc = crc16(data, len);
    digitalWrite(PIN_DE_RE, HIGH);
    Serial1.write(data, len);
    Serial1.write(crc & 0xFF);
    Serial1.write(crc >> 8);
    Serial1.flush();
    delayMicroseconds(100);
    digitalWrite(PIN_DE_RE, LOW);
}

void ModbusRTU::respondException(uint8_t fc, uint8_t code) {
    uint8_t resp[3] = {_slave_id, (uint8_t)(fc | 0x80), code};
    sendResponse(resp, 3);
}

void ModbusRTU::respondFC03(uint8_t *frame, Settings &cfg) {
    uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t count = ((uint16_t)frame[4] << 8) | frame[5];

    if (count < 1 || count > 3) { respondException(0x03, 0x03); return; }

    uint8_t resp[9];
    resp[0] = _slave_id;
    resp[1] = 0x03;
    resp[2] = count * 2;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val = 0;
        switch (start + i) {
            case HR_TARGET_POS: val = (uint16_t)_target_pct; break;
            case HR_SLAVE_ID:   val = cfg.slave_id;           break;
            case HR_BAUD_CODE:  val = cfg.baud_code;          break;
            default: respondException(0x03, 0x02); return;
        }
        resp[3 + i*2] = val >> 8;
        resp[4 + i*2] = val & 0xFF;
    }
    sendResponse(resp, 3 + count * 2);
}

void ModbusRTU::respondFC04(uint8_t *frame, float pos_pct, uint16_t status, float current_a) {
    uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t count = ((uint16_t)frame[4] << 8) | frame[5];

    if (count < 1 || count > 3) { respondException(0x04, 0x03); return; }

    uint8_t resp[9];
    resp[0] = _slave_id;
    resp[1] = 0x04;
    resp[2] = count * 2;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val = 0;
        switch (start + i) {
            case IR_CURRENT_POS: val = (uint16_t)pos_pct;              break;
            case IR_STATUS:      val = status;                          break;
            case IR_CURRENT_MA:  val = (uint16_t)(current_a * 10.0f);  break;
            default: respondException(0x04, 0x02); return;
        }
        resp[3 + i*2] = val >> 8;
        resp[4 + i*2] = val & 0xFF;
    }
    sendResponse(resp, 3 + count * 2);
}

void ModbusRTU::respondFC06(uint8_t *frame, Settings &cfg, bool broadcast) {
    uint16_t addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t val  = ((uint16_t)frame[4] << 8) | frame[5];

    switch (addr) {
        case HR_TARGET_POS:
            if (val > 100) { if (!broadcast) respondException(0x06, 0x03); return; }
            _target_pct = val;
            _new_target = true;
            break;
        case HR_SLAVE_ID:
            if (val < 1 || val > 247) { if (!broadcast) respondException(0x06, 0x03); return; }
            cfg.slave_id = (uint8_t)val;
            flashStorage.save(cfg);
            break;
        case HR_BAUD_CODE:
            if (val >= BAUD_TABLE_SIZE) { if (!broadcast) respondException(0x06, 0x03); return; }
            cfg.baud_code = (uint8_t)val;
            flashStorage.save(cfg);
            break;
        default:
            if (!broadcast) respondException(0x06, 0x02);
            return;
    }
    // 에코 응답 (브로드캐스트는 응답 없음)
    if (!broadcast) sendResponse(frame, 6);
}

void ModbusRTU::respondFC16(uint8_t *frame, Settings &cfg, bool broadcast) {
    uint16_t start     = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t count     = ((uint16_t)frame[4] << 8) | frame[5];
    uint8_t  byte_cnt  = frame[6];

    if (byte_cnt != count * 2) { if (!broadcast) respondException(0x10, 0x03); return; }

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val  = ((uint16_t)frame[7 + i*2] << 8) | frame[8 + i*2];
        uint16_t addr = start + i;
        switch (addr) {
            case HR_TARGET_POS:
                if (val > 100) { if (!broadcast) respondException(0x10, 0x03); return; }
                _target_pct = val;
                _new_target = true;
                break;
            case HR_SLAVE_ID:
                if (val < 1 || val > 247) { if (!broadcast) respondException(0x10, 0x03); return; }
                cfg.slave_id = (uint8_t)val;
                break;
            case HR_BAUD_CODE:
                if (val >= BAUD_TABLE_SIZE) { if (!broadcast) respondException(0x10, 0x03); return; }
                cfg.baud_code = (uint8_t)val;
                break;
            default:
                if (!broadcast) respondException(0x10, 0x02);
                return;
        }
    }
    flashStorage.save(cfg);

    // 응답 (브로드캐스트는 응답 없음)
    if (!broadcast) {
        uint8_t resp[6];
        resp[0] = _slave_id;
        resp[1] = 0x10;
        resp[2] = frame[2]; resp[3] = frame[3];
        resp[4] = frame[4]; resp[5] = frame[5];
        sendResponse(resp, 6);
    }
}

void ModbusRTU::processFrame(uint8_t *frame, uint8_t len,
                              Settings &cfg, float pos_pct,
                              uint16_t status, float current_a) {
    if (len < 4) return;
    if (frame[0] != _slave_id && frame[0] != 0x00) return;

    uint16_t recv_crc = ((uint16_t)frame[len-1] << 8) | frame[len-2];
    if (crc16(frame, len - 2) != recv_crc) return;

    // 브로드캐스트(0x00)는 쓰기 명령만 처리, 응답 없음
    bool broadcast = (frame[0] == 0x00);

    uint8_t fc = frame[1];
    switch (fc) {
        case 0x03: if (!broadcast) respondFC03(frame, cfg);                          break;
        case 0x04: if (!broadcast) respondFC04(frame, pos_pct, status, current_a);  break;
        case 0x06: respondFC06(frame, cfg, broadcast); break;
        case 0x10: respondFC16(frame, cfg, broadcast); break;
        default:   if (!broadcast) respondException(fc, 0x01); break;
    }
}

void ModbusRTU::update(Settings &cfg, float pos_pct, uint16_t status, float current_a) {
    while (Serial1.available()) {
        if (_buf_len < sizeof(_buf)) {
            _buf[_buf_len++] = Serial1.read();
        } else {
            Serial1.read();  // 버퍼 초과 시 버림
        }
        _last_rx_us = micros();  // 바이트 수신 즉시 시각 갱신
    }

    uint32_t now_us = micros();

    // 3.5 문자 시간 침묵 → 프레임 완성
    if (_buf_len > 0) {
        uint32_t baud = BAUD_TABLE[cfg.baud_code];
        if ((now_us - _last_rx_us) > INTER_FRAME_US(baud)) {
            processFrame(_buf, _buf_len, cfg, pos_pct, status, current_a);
            _buf_len = 0;
        }
    }
}
