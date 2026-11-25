#pragma once

#include <cstdint>
#include <portmacro.h>

using BaseType_t = int32_t;
using UBaseType_t = uint32_t;
using TickType_t = uint32_t;

constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
constexpr UBaseType_t portBYTE_ALIGNMENT = 8;
