#include <Arduino.h>
#include <esp_task_wdt.h>
#include <string.h>
#include "app_ControlTask.h"
#include "app_Dispatcher.h"
#include "dev_Compat.h"
#include "dev_Sensor.h"
#include "dev_MotorDriver.h"
#include "dev_Relay.h"
#include "dev_TempSensor.h"
#include "pins.h"

namespace {

const uint8_t QUEUE_LEN = 8;

QueueHandle_t     s_jobs = nullptr;
QueueHandle_t     s_alerts = nullptr;
SemaphoreHandle_t s_statusMutex = nullptr;
SemaphoreHandle_t s_settingsMutex = nullptr;
volatile bool     s_alertOverflow = false;

Status      s_status;          // 상태 복사본 (뮤텍스로 보호)
Status      s_work;            // 제어 태스크가 채우는 쪽
Settings    s_pending;         // 화면 태스크가 넘긴 새 설정 (뮤텍스로 보호)
uint8_t     s_pendTimes = 0, s_pendCal = 0;

Dispatcher  s_disp;
HomeSensor  s_sensor[2];
MotorDriver s_motor[2];
RelayDriver s_relay;
TempSensor  s_temp;

void setupWatchdog() {
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t cfg = {};
    cfg.timeout_ms = WATCHDOG_TIMEOUT_S * 1000;
    cfg.idle_core_mask = 0;
    cfg.trigger_panic = true;
    if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(nullptr);
}

void controlLoop(void*) {
    setupWatchdog();
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t now = millis();

        // 1. 얼럿을 전부 비우며 처리. 큐가 넘쳤으면 전체 정지로 본다
        bool hadAlert = false;
        Alert a;
        while (xQueueReceive(s_alerts, &a, 0) == pdTRUE) { s_disp.alert(a, now); hadAlert = true; }
        if (s_alertOverflow) {
            s_alertOverflow = false;
            Alert stop = { AlertType::STOP_ALL, 0xFF, Cause::NONE };
            s_disp.alert(stop, now);
            hadAlert = true;
        }

        // 2. 장치를 직접 읽어 안전 판정과 상태 흐름
        ControlInputs in;
        in.now = now;
        for (uint8_t i = 0; i < 2; i++) {
            in.sensor[i] = s_sensor[i].update(now);
            in.contactsValid[i] = s_sensor[i].contactsValid();
            s_motor[i].sense();
            in.currentMa[i] = s_motor[i].currentMa();
            in.driverFault[i] = s_motor[i].driverFault();
        }
        in.relayMv = s_relay.relayMv();
        in.estopMv = s_relay.estopMv();
        in.tempUa = s_temp.readUa();

        ControlOutputs out = s_disp.step(in);
        for (uint8_t i = 0; i < 2; i++) s_motor[i].drive(out.dir[i], out.duty[i], now);
        s_relay.set(out.fetA, out.fetB);

        // 3. 얼럿이 없었으면 잡 하나. 전원 켤 때 점검이 끝나기 전에는 꺼내지 않는다
        if (!hadAlert && s_disp.bootDone()) {
            Job j;
            if (xQueueReceive(s_jobs, &j, 0) == pdTRUE) {
                if (j.type == JobType::SETTINGS_CHANGED) {
                    Settings s;
                    uint8_t tm, cm;
                    xSemaphoreTake(s_settingsMutex, portMAX_DELAY);
                    s = s_pending; tm = s_pendTimes; cm = s_pendCal;
                    s_pendTimes = 0; s_pendCal = 0;
                    xSemaphoreGive(s_settingsMutex);
                    s_disp.settingsChanged(s, tm, cm);
                } else {
                    s_disp.job(j, now);
                }
            }
        }

        // 4. 상태 복사본과 워치독
        s_disp.fillStatus(s_work);
        if (xSemaphoreTake(s_statusMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            memcpy(&s_status, &s_work, sizeof(Status));
            xSemaphoreGive(s_statusMutex);
        }
        esp_task_wdt_reset();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

}  // namespace

namespace ControlTask {

void begin(Model model, const Settings& s) {
    if (!s_jobs) {
        s_jobs = xQueueCreate(QUEUE_LEN, sizeof(Job));
        s_alerts = xQueueCreate(QUEUE_LEN, sizeof(Alert));
        s_statusMutex = xSemaphoreCreateMutex();
        s_settingsMutex = xSemaphoreCreateMutex();
    }
    uint32_t now = millis();
    int32_t ohm = model == Model::W50 ? SENSE_OHM_50W : SENSE_OHM_300W;
    s_relay.begin(PIN_RELAY_FET_A, PIN_RELAY_FET_B, PIN_RELAY_SENSE, PIN_ESTOP_SENSE);
    s_motor[0].begin(PIN_M1_RPWM, PIN_M1_LPWM, PIN_M1_EN, PIN_M1_CURRENT, LEDC_CH_M1_R, LEDC_CH_M1_L, ohm, now);
    s_motor[1].begin(PIN_M2_RPWM, PIN_M2_LPWM, PIN_M2_EN, PIN_M2_CURRENT, LEDC_CH_M2_R, LEDC_CH_M2_L, ohm, now);
    s_sensor[0].begin(PIN_S1_NO, PIN_S1_NC, now);
    s_sensor[1].begin(PIN_S2_NO, PIN_S2_NC, now);
    s_temp.begin(PIN_TEMP_SENSE);
    s_pending = s;
    s_disp.begin(model, s, now);
    s_disp.fillStatus(s_status);
    xTaskCreatePinnedToCore(controlLoop, "control", 8192, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
}

bool sendJob(const Job& j) {
    return s_jobs && xQueueSend(s_jobs, &j, 0) == pdTRUE;
}

bool sendAlert(const Alert& a) {
    if (!s_alerts) return false;
    if (xQueueSend(s_alerts, &a, 0) == pdTRUE) return true;
    s_alertOverflow = true;         // 가득 참: 제어 쪽에서 전체 정지
    return false;
}

bool sendSettings(const Settings& s, uint8_t timesMask, uint8_t calMask) {
    xSemaphoreTake(s_settingsMutex, portMAX_DELAY);
    s_pending = s;
    s_pendTimes |= timesMask;
    s_pendCal |= calMask;
    xSemaphoreGive(s_settingsMutex);
    Job j = {};
    j.type = JobType::SETTINGS_CHANGED;
    j.source = JobSource::ENCODER;
    j.timesMask = timesMask;
    j.calMask = calMask;
    return sendJob(j);
}

void getStatus(Status& out) {
    xSemaphoreTake(s_statusMutex, portMAX_DELAY);
    memcpy(&out, &s_status, sizeof(Status));
    xSemaphoreGive(s_statusMutex);
}

}  // namespace ControlTask
