#pragma once
#include <cstdint>

struct VramHandle {
    uint64_t id = 0;
    constexpr bool isValid() const { return id != 0; }
};