#pragma once
#include <cstdint>
#include <functional>

// Handle do identyfikacji pipeline'ów
struct PipelineHandle {
    uint32_t id;
    explicit PipelineHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const PipelineHandle& other) const { return id == other.id; }
};

// Hash function dla PipelineHandle
namespace std {
    template <>
    struct hash<PipelineHandle> {
        size_t operator()(const PipelineHandle& h) const {
            return hash<uint32_t>()(h.id);
        }
    };
}