#pragma once
#include "services.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

namespace services::detail {
extern wxconnect::Settings config;
extern wxconnect::State current;
SemaphoreHandle_t state_mutex = nullptr;
QueueHandle_t observations = nullptr;
Observation latest;
std::atomic<bool> http_serving{false};
void startWorker();
void writeState(const wxconnect::State& next);
}
