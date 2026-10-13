#pragma once
#include <stdint.h>
typedef void* esp_timer_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
typedef struct { void (*callback)(void*); void* arg; int dispatch_method; const char* name; bool skip_unhandled_events; } esp_timer_create_args_t;
inline esp_err_t esp_timer_create(const esp_timer_create_args_t*, esp_timer_handle_t*) { return 0; }
inline esp_err_t esp_timer_start_once(esp_timer_handle_t, uint64_t) { return 0; }
inline esp_err_t esp_timer_stop(esp_timer_handle_t) { return 0; }
