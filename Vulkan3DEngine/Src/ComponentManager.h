#pragma once
#include "Component.h"
#include <unordered_map>
#include <string>
#include <typeindex>
#include <memory>
#include <vector>
#include <stdexcept>
#include <functional>
#include <json.hpp>
#include "Entity.h"

// Forward declaration to avoid circular dependency
class Registry;

class ComponentManager {
public:
    explicit ComponentManager(Registry& registry);

    // Component registration
    template<typename T>
    void registerComponent();

    // Component lifecycle
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args);

    template<typename T>
    void removeComponent(Entity entity);

    template<typename T>
    T& getComponent(Entity entity);

    template<typename T>
    bool hasComponent(Entity entity) const;

    // Entity queries
    template<typename... Components>
    std::vector<Entity> createView(const std::vector<Entity>& entities);

    // Entity component introspection
    std::vector<std::string> getEntityComponentTypes(Entity entity) const;
    bool hasAnyComponents(Entity entity) const;

    // Component removal for entity destruction
    void removeAllComponents(Entity entity);

    // Serialization support
    nlohmann::json serializeComponent(Entity entity, const std::string& componentTypeName) const;
    void deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData);

    // Component factory (for deserialization)
    bool createComponentFromData(Entity entity, const std::string& componentName, const nlohmann::json& data);

    // Component registry queries
    bool isComponentRegistered(const std::string& name) const;

    template<typename T>
    std::string getComponentName() const;

    std::string getComponentName(std::type_index type) const;
    std::type_index getComponentType(const std::string& name) const;
    std::vector<std::string> getAllComponentNames() const;
    size_t getComponentCount() const;

private:
    void initializeComponents();

    // Component pool interface
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
        virtual nlohmann::json serializeComponent(Entity entity) const = 0;
        virtual void deserializeComponent(Entity entity, const nlohmann::json& data) = 0;
        virtual bool hasEntity(Entity entity) const = 0;
    };

    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        explicit ComponentPool(Registry& registry) : m_registry(registry) {}

        std::vector<T> m_components;
        std::unordered_map<Entity, size_t> m_entityToIndex;
        std::unordered_map<size_t, Entity> m_indexToEntity;

        T& add(Entity entity, T&& component);
        void remove(Entity entity) override;

        nlohmann::json serializeComponent(Entity entity) const override;
        void deserializeComponent(Entity entity, const nlohmann::json& data) override;
        bool hasEntity(Entity entity) const override;

    private:
        Registry& m_registry;
    };

    using DeserializerFunction = std::function<void(Entity, const nlohmann::json&)>;

    Registry& m_registry; // Reference to Registry
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_componentPools;
    std::unordered_map<std::type_index, std::string> m_componentTypes;
    std::unordered_map<std::string, std::type_index> m_nameToType;
    std::unordered_map<std::string, DeserializerFunction> m_deserializerFunctions;

    // Helper method
    IComponentPool* getComponentPool(const std::string& componentTypeName) const;
};

// Template implementations
template<typename T>
void ComponentManager::registerComponent() {
    static_assert(std::is_base_of_v<Component, T>,
        "Registered type must be a Component");

    auto type = std::type_index(typeid(T));
    if (m_componentTypes.find(type) != m_componentTypes.end()) {
        throw std::runtime_error("Component already registered");
    }

    // Create temporary instance to get name
    T tempComponent;
    std::string name = tempComponent.getName();

    m_componentTypes[type] = name;
    m_nameToType.emplace(name, type);

    // Register factory function for deserialization
    m_deserializerFunctions[name] = [this](Entity entity, const nlohmann::json& data) {
        T component;
        component.deserialize(data);
        addComponent<T>(entity, std::move(component));
        };
}

template<typename T, typename... Args>
T& ComponentManager::addComponent(Entity entity, Args&&... args) {
    auto type = std::type_index(typeid(T));
    auto poolIt = m_componentPools.find(type);

    if (poolIt == m_componentPools.end()) {
        auto pool = std::make_unique<ComponentPool<T>>(m_registry);
        poolIt = m_componentPools.emplace(type, std::move(pool)).first;
    }

    auto& pool = static_cast<ComponentPool<T>&>(*poolIt->second);
    return pool.add(entity, T(std::forward<Args>(args)...));
}

template<typename T>
void ComponentManager::removeComponent(Entity entity) {
    auto type = std::type_index(typeid(T));
    if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
        it->second->remove(entity);
    }
}

template<typename T>
T& ComponentManager::getComponent(Entity entity) {
    auto type = std::type_index(typeid(T));
    auto& pool = static_cast<ComponentPool<T>&>(*m_componentPools.at(type));
    return pool.m_components[pool.m_entityToIndex.at(entity)];
}

template<typename T>
bool ComponentManager::hasComponent(Entity entity) const {
    auto type = std::type_index(typeid(T));
    if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
        auto& pool = static_cast<const ComponentPool<T>&>(*it->second);
        return pool.m_entityToIndex.count(entity) > 0;
    }
    return false;
}

template<typename... Components>
std::vector<Entity> ComponentManager::createView(const std::vector<Entity>& entities) {
    std::vector<Entity> result;
    for (Entity entity : entities) {
        bool hasAll = (hasComponent<Components>(entity) && ...);
        if (hasAll) result.push_back(entity);
    }
    return result;
}

template<typename T>
std::string ComponentManager::getComponentName() const {
    auto type = std::type_index(typeid(T));
    auto it = m_componentTypes.find(type);
    if (it != m_componentTypes.end()) {
        return it->second;
    }
    throw std::runtime_error("Component not registered");
}

template<typename T>
T& ComponentManager::ComponentPool<T>::add(Entity entity, T&& component) {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");

    component.entity = entity;
    // Set registry reference directly here
    component.setRegistry(&m_registry);

    size_t index = m_components.size();
    m_entityToIndex[entity] = index;
    m_indexToEntity[index] = entity;
    m_components.emplace_back(std::move(component));

    return m_components.back();
}

template<typename T>
void ComponentManager::ComponentPool<T>::remove(Entity entity) {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) return;

    size_t index = it->second;
    size_t lastIndex = m_components.size() - 1;

    if (index != lastIndex) {
        m_components[index] = std::move(m_components[lastIndex]);
        Entity movedEntity = m_indexToEntity[lastIndex];
        m_entityToIndex[movedEntity] = index;
        m_indexToEntity[index] = movedEntity;
    }

    m_components.pop_back();
    m_entityToIndex.erase(entity);
    m_indexToEntity.erase(lastIndex);
}

template<typename T>
nlohmann::json ComponentManager::ComponentPool<T>::serializeComponent(Entity entity) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        throw std::runtime_error("Entity not found in component pool");
    }
    return m_components[it->second].serialize();
}

template<typename T>
void ComponentManager::ComponentPool<T>::deserializeComponent(Entity entity, const nlohmann::json& data) {
    auto it = m_entityToIndex.find(entity);
    if (it != m_entityToIndex.end()) {
        m_components[it->second].deserialize(data);
        // Ensure registry is set after deserialization
        m_components[it->second].setRegistry(&m_registry);
    }
    else {
        throw std::runtime_error("Entity not found in component pool for deserialization");
    }
}

template<typename T>
bool ComponentManager::ComponentPool<T>::hasEntity(Entity entity) const {
    return m_entityToIndex.find(entity) != m_entityToIndex.end();
}