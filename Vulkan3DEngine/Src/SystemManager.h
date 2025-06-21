#pragma once
#include "System.h"
#include "ISerializable.h"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>

class SystemManager : public ISerializable {
public:
    SystemManager(Engine& engine, Registry& registry)
        : m_engine(engine), m_registry(registry) {
        initializeSystems();
    }

    template<typename T>
    void registerSystem() {
        static_assert(std::is_base_of_v<ISystem, T>,
            "Registered type must be a System");
        auto type = std::type_index(typeid(T));
        if (m_systems.find(type) != m_systems.end()) {
            throw std::runtime_error("System already registered");
        }
        m_systems[type] = std::make_unique<T>();
        m_systemActive[type] = false; // Domyślnie nieaktywny

        // Wywołaj initialize dla nowo zarejestrowanego systemu
        m_systems[type]->initialize(*this, m_registry, m_engine);
    }

    template<typename T>
    void registerSystemActive() {
        registerSystem<T>();
        setSystemActive<T>(true);
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

    template<typename T>
    void setSystemActive(bool active) {
        auto type = std::type_index(typeid(T));
        if (m_systems.find(type) == m_systems.end()) {
            throw std::runtime_error("System not registered");
        }
        m_systemActive[type] = active;
    }

    template<typename T>
    bool isSystemActive() const {
        auto type = std::type_index(typeid(T));
        auto it = m_systemActive.find(type);
        return it != m_systemActive.end() ? it->second : false;
    }

    void updateAll() {
        for (auto& [type, system] : m_systems) {
            if (m_systemActive[type]) {
                system->update();
            }
        }
    }

    // Dodatkowe metody użyteczne do debugowania
    size_t getSystemCount() const { return m_systems.size(); }
    size_t getActiveSystemCount() const {
        size_t count = 0;
        for (const auto& [type, active] : m_systemActive) {
            if (active) count++;
        }
        return count;
    }

    // ISerializable implementation
    json serialize() const override;
    void deserialize(const json& j) override;

private:
    void initializeSystems();

    Registry& m_registry;
    Engine& m_engine;
    std::unordered_map<std::type_index, std::unique_ptr<ISystem>> m_systems;
    std::unordered_map<std::type_index, bool> m_systemActive;
};