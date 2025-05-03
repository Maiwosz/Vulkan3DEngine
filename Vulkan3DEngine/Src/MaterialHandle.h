#pragma once
#include <cstdint>

struct MaterialHandle {
    uint32_t id;
    constexpr explicit MaterialHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const MaterialHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function for MaterialHandle
namespace std {
    template <>
    struct hash<MaterialHandle> {
        size_t operator()(const MaterialHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}