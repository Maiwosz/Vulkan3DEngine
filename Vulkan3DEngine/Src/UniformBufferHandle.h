#pragma once
#include <cstdint>
#include <functional>

// Handle do identyfikacji Uniform Buffer
struct UniformBufferHandle {
    uint32_t id;
    constexpr explicit UniformBufferHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const UniformBufferHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function dla UniformBufferHandle
namespace std {
    template <>
    struct hash<UniformBufferHandle> {
        size_t operator()(const UniformBufferHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}