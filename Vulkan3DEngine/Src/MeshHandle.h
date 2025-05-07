#pragma once
#include <cstdint>
#include <functional>

struct MeshHandle {
    uint32_t id;
    constexpr explicit MeshHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const MeshHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function for MeshHandle
namespace std {
    template <>
    struct hash<MeshHandle> {
        size_t operator()(const MeshHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}