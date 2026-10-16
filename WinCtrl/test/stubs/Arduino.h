#pragma once
// PC에서 아두이노 쪽 파일의 문법만 확인하기 위한 가짜 헤더. 동작하지 않는다.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define CHANGE 3
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 3
#endif
#ifndef ESP_IDF_VERSION_MAJOR
#define ESP_IDF_VERSION_MAJOR 5
#endif
typedef enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db } adc_attenuation_t;
struct HardwareSerial {
    void begin(unsigned long) {}
    int printf(const char*, ...) __attribute__((format(printf, 2, 3))) { return 0; }
    size_t println(const char*) { return 0; }
};
static HardwareSerial Serial;
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return 0; }
inline uint32_t analogReadMilliVolts(uint8_t) { return 0; }
inline void analogSetPinAttenuation(uint8_t, adc_attenuation_t) {}
inline unsigned long millis() { return 0; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
inline bool ledcWrite(uint8_t, uint32_t) { return true; }
inline uint32_t ledcWriteTone(uint8_t, uint32_t) { return 0; }
inline int digitalPinToInterrupt(uint8_t p) { return p; }
inline void attachInterrupt(int, void (*)(), int) {}
inline int64_t esp_timer_get_time() { return 0; }
typedef struct { int x; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}
inline void portENTER_CRITICAL(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE*) {}
// FreeRTOS
typedef uint32_t TickType_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFu
#define configMAX_PRIORITIES 25
#define pdMS_TO_TICKS(x) (x)
inline TickType_t xTaskGetTickCount() { return 0; }
inline void vTaskDelayUntil(TickType_t*, TickType_t) {}
inline void vTaskDelete(void*) {}
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, int, TaskHandle_t*, int) { return 1; }
inline QueueHandle_t xQueueCreate(int, size_t) { return nullptr; }
inline BaseType_t xQueueSend(QueueHandle_t, const void*, TickType_t) { return 1; }
inline BaseType_t xQueueReceive(QueueHandle_t, void*, TickType_t) { return 0; }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return nullptr; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return 1; }
// 코어 2.x 함수
inline uint32_t ledcSetup(uint8_t, uint32_t, uint8_t) { return 0; }
inline void ledcAttachPin(uint8_t, uint8_t) {}
