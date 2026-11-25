#pragma once

#include <cstdint>

inline uint32_t testMillis = 0;
inline uint32_t millis() { return testMillis; }

class TestSerial {
public:
    template <typename... Args>
    void printf(const char*, Args...) const {}
};

extern TestSerial Serial;
