#include "dev_Compat.h"
#include "dev_Buzzer.h"
#include <esp_timer.h>

static void buzzerTimerCb(void* arg) { static_cast<Buzzer*>(arg)->onTimer(); }

void Buzzer::begin(uint8_t pin) {
    _pin = pin;
    pwmAttach(_pin, LEDC_CH_BUZZER, 2000, PWM_BITS);
    pwmWrite(_pin, LEDC_CH_BUZZER, 0);
    esp_timer_create_args_t args = {};
    args.callback = buzzerTimerCb;
    args.arg = this;
    args.name = "buzzer";
    esp_timer_handle_t h = nullptr;
    if (esp_timer_create(&args, &h) == ESP_OK) _timer = h;
}

void Buzzer::tone(uint16_t hz, uint16_t ms) {
    esp_timer_handle_t h = static_cast<esp_timer_handle_t>(_timer);
    if (h) esp_timer_stop(h);
    pwmTone(_pin, LEDC_CH_BUZZER, hz);
    _busy = true;
    if (h) esp_timer_start_once(h, (uint64_t)ms * 1000);
}

void Buzzer::off() {
    esp_timer_handle_t h = static_cast<esp_timer_handle_t>(_timer);
    if (h) esp_timer_stop(h);
    onTimer();
}

void Buzzer::onTimer() {
    pwmTone(_pin, LEDC_CH_BUZZER, 0);
    _busy = false;
}
