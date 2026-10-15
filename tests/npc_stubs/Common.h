#pragma once
#include <cstdint>
using uint32 = std::uint32_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;

using uint64 = std::uint64_t;
struct ObjectGuid { uint32 id = 42; uint32 GetCounter() const { return id; } };
