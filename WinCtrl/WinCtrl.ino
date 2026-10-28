// ============================================================
//  측창 모터 제어기 펌웨어 0.9 (ESP32-S3)
//  기준 문서: Google 드라이브 "측창 모터 제어기" 폴더의
//  소프트웨어 스펙 v0.9, 핀 배정 (ESP32-S3), 설계 정리.
//
//  setup()은 저장된 값을 읽고 두 태스크를 띄운다. loop()는 쓰지 않는다.
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "Types.h"
#include "Settings.h"
#include "Store.h"
#include "ErrorLog.h"
#include "app_ControlTask.h"
#include "io_UiTask.h"
#include <Wire.h>

static ErrorLog s_bootLog;

// 시리얼 확인용 로그. COM(UART) 포트, 115200bps.
// LCD가 안 나올 때 보드가 살아 있는지, I2C에 무엇이 붙어 있는지 본다.
static void scanI2c() {
    // 버스가 쉬는 상태면 풀업 때문에 두 선 모두 HIGH(1)여야 한다.
    // 0이면 풀업이 없거나(레벨 변환 모듈 전원) 선이 GND 쪽으로 눌려 있다.
    pinMode(PIN_LCD_SDA, INPUT);
    pinMode(PIN_LCD_SCL, INPUT);
    delay(2);
    int sda = digitalRead(PIN_LCD_SDA), scl = digitalRead(PIN_LCD_SCL);
    Serial.printf("[boot] I2C idle level SDA(8)=%d SCL(9)=%d %s\n", sda, scl,
                  (sda && scl) ? "OK" : "<- should be 1, check pull-up / level shifter power");
    bool ok = Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL, 100000);
    Serial.printf("[boot] I2C begin %s, scanning...\n", ok ? "OK" : "FAILED");
    Wire.setTimeOut(20);
    uint8_t found = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[boot] I2C device at 0x%02X\n", a);
            found++;
        }
    }
    if (found == 0) Serial.println("[boot] I2C: no device (check SDA=8, SCL=9, level shifter, LCD 5V)");
    Wire.end();
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n[boot] %s %02u.%02u  build %s %s\n",
                  PRODUCT_NAME, FW_VERSION_MAJOR, FW_VERSION_MINOR, __DATE__, __TIME__);
#ifdef WINCTRL_BENCH
    Serial.println("[boot] !!! BENCH BUILD: relay voltage check OFF. Do not install. !!!");
#endif
#ifdef WINCTRL_UISIM
    Serial.println("[boot] !!! UI SIM BUILD: motors, relay, E-STOP, sensors are FAKE. Do not install. !!!");
#endif

    // 모델 구분 점퍼: 열림(HIGH) 300W, GND(LOW) 50W
    pinMode(PIN_MODEL_JUMPER, INPUT_PULLUP);
    delay(2);
    Model model = digitalRead(PIN_MODEL_JUMPER) == LOW ? Model::W50 : Model::W300;

    Store::begin();
    Settings s;
    Store::loadSettings(s);
    uint16_t boot = Store::bumpBootCount();
    Store::loadErrors(s_bootLog);
    Serial.printf("[boot] model %s, boot #%u, stored errors %u\n",
                  model == Model::W50 ? "50W" : "300W", boot, s_bootLog.count());
    scanI2c();

    ControlTask::begin(model, s);
    UiTask::begin(model, s, s_bootLog, boot, __DATE__, __TIME__);   // 컴파일 날짜와 시각
    Serial.println("[boot] tasks started");
}

void loop() {
    // 할 일은 두 태스크가 한다. 여기서는 살아 있다는 표시만 5초마다 찍는다.
    Serial.printf("[alive] %lus  enc A(41)=%d B(42)=%d SW(47)=%d\n", (unsigned long)(millis() / 1000),
                  digitalRead(PIN_ENC_A), digitalRead(PIN_ENC_B), digitalRead(PIN_ENC_SW));
    delay(5000);
}
