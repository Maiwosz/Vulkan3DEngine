#pragma once
#include <cstdint>
#include <functional>

// Handle do identyfikacji pipeline layout'ów
struct PipelineLayoutHandle {
    uint32_t id;
    constexpr explicit PipelineLayoutHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const PipelineLayoutHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function dla PipelineLayoutHandle
namespace std {
    template <>
    struct hash<PipelineLayoutHandle> {
        size_t operator()(const PipelineLayoutHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}