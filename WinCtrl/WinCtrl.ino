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

static ErrorLog s_bootLog;

void setup() {
    // 모델 구분 점퍼: 열림(HIGH) 300W, GND(LOW) 50W
    pinMode(PIN_MODEL_JUMPER, INPUT_PULLUP);
    delay(2);
    Model model = digitalRead(PIN_MODEL_JUMPER) == LOW ? Model::W50 : Model::W300;

    Store::begin();
    Settings s;
    Store::loadSettings(s);
    uint16_t boot = Store::bumpBootCount();
    Store::loadErrors(s_bootLog);

    ControlTask::begin(model, s);
    UiTask::begin(model, s, s_bootLog, boot, __DATE__, __TIME__);   // 컴파일 날짜와 시각
}

void loop() {
    vTaskDelete(nullptr);    // 할 일은 두 태스크가 한다
}
