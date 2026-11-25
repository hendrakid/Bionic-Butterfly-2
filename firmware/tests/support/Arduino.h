#pragma once

class TestSerial {
public:
    template <typename... Args>
    void printf(const char*, Args...) const {}
};

extern TestSerial Serial;
