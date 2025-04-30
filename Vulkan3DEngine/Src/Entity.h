#pragma once
#include <cstdint>
#include <functional>

struct Entity {
    uint32_t id;
    explicit Entity(uint32_t id = 0) : id(id) {}

    operator uint32_t() const { return id; }
    bool operator==(const Entity& other) const { return id == other.id; }
};


namespace std {
    template <>
    struct hash<Entity> {
        size_t operator()(const Entity& e) const noexcept {
            return hash<uint32_t>()(e.id);
        }
    };
}