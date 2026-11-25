#pragma once

#include <cstdint>

using portMUX_TYPE = uint32_t;
#define portMUX_INITIALIZER_UNLOCKED 0U
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
