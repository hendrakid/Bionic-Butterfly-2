#pragma once

#include <freertos/FreeRTOS.h>
#include <cstddef>
#include <cstdint>

struct StaticQueue_t {
    uint8_t* storage = nullptr;
    size_t capacity = 0;
    size_t itemSize = 0;
    size_t head = 0;
    size_t count = 0;
};

using QueueHandle_t = StaticQueue_t*;

QueueHandle_t xQueueCreateStatic(UBaseType_t length,
                                 UBaseType_t itemSize,
                                 uint8_t* storage,
                                 StaticQueue_t* queue);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
