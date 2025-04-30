#pragma once
#include <cstdint>
#include <functional>

// Define handles for shader modules and pipeline layouts
struct ShaderModuleHandle {
    uint32_t id;
    constexpr explicit ShaderModuleHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const ShaderModuleHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template <>
    struct hash<ShaderModuleHandle> {
        size_t operator()(const ShaderModuleHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}
