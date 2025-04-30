#pragma once
#include "System.h"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>

class SystemManager {
public:
    SystemManager(Registry& registry) : m_registry(registry) {}

    template<typename T>
    void registerSystem() {
        static_assert(std::is_base_of_v<ISystem, T>,
            "Registered type must be a System");
        auto type = std::type_index(typeid(T));
        if (m_systems.find(type) != m_systems.end()) {
            throw std::runtime_error("System already registered");
        }
        m_systems[type] = std::make_unique<T>();
    }

    template<typename T>
    T& getSystem() {
        auto type = std::type_index(typeid(T));
        auto it = m_systems.find(type);
        if (it == m_systems.end()) {
            throw std::runtime_error("System not registered");
        }
        return static_cast<T&>(*it->second);
    }

    void updateAll() {
        for (auto& [type, system] : m_systems) {
            system->update(*this, m_registry);
        }
    }

private:
    Registry& m_registry;
    std::unordered_map<std::type_index, std::unique_ptr<ISystem>> m_systems;
};