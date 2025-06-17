#pragma once
#include "Component.h"
#include <unordered_map>
#include <string>
#include <typeindex>
#include <memory>
#include <vector>
#include <stdexcept>

class Registry; // Forward declaration

class ComponentManager {
public:
    ComponentManager(Registry& registry) : m_registry(registry) {
        initializeComponents();
    }

    template<typename T>
    void registerComponent() {
        static_assert(std::is_base_of_v<Component, T>,
            "Registered type must be a Component");

        auto type = std::type_index(typeid(T));
        if (m_componentTypes.find(type) != m_componentTypes.end()) {
            throw std::runtime_error("Component already registered");
        }

        // Tworzymy tymczasową instancję żeby uzyskać nazwę
        T tempComponent;
        std::string name = tempComponent.getName(); // Poprawione z getComponentName na getName

        m_componentTypes[type] = name;
        // FIX: Use insert() or emplace() instead of operator[]
        m_nameToType.insert({ name, type });
    }

    // Uzyskanie nazwy komponentu na podstawie typu
    template<typename T>
    std::string getComponentName() const {
        auto type = std::type_index(typeid(T));
        auto it = m_componentTypes.find(type);
        if (it != m_componentTypes.end()) {
            return it->second;
        }
        throw std::runtime_error("Component not registered");
    }

    // Uzyskanie nazwy komponentu na podstawie type_index
    std::string getComponentName(std::type_index type) const {
        auto it = m_componentTypes.find(type);
        if (it != m_componentTypes.end()) {
            return it->second;
        }
        throw std::runtime_error("Component type not registered");
    }

    // Uzyskanie typu na podstawie nazwy
    std::type_index getComponentType(const std::string& name) const {
        auto it = m_nameToType.find(name);
        if (it != m_nameToType.end()) {
            return it->second;
        }
        throw std::runtime_error("Component name not found: " + name);
    }

    // Sprawdzenie czy komponent jest zarejestrowany
    template<typename T>
    bool isComponentRegistered() const {
        auto type = std::type_index(typeid(T));
        return m_componentTypes.find(type) != m_componentTypes.end();
    }

    bool isComponentRegistered(const std::string& name) const {
        return m_nameToType.find(name) != m_nameToType.end();
    }

    bool isComponentRegistered(std::type_index type) const {
        return m_componentTypes.find(type) != m_componentTypes.end();
    }

    // Uzyskanie listy wszystkich zarejestrowanych komponentów
    std::vector<std::string> getAllComponentNames() const {
        std::vector<std::string> names;
        names.reserve(m_componentTypes.size());
        for (const auto& [type, name] : m_componentTypes) {
            names.push_back(name);
        }
        return names;
    }

    // Liczba zarejestrowanych komponentów
    size_t getComponentCount() const { return m_componentTypes.size(); }

private:
    void initializeComponents();

    Registry& m_registry;
    std::unordered_map<std::type_index, std::string> m_componentTypes;
    std::unordered_map<std::string, std::type_index> m_nameToType;
};