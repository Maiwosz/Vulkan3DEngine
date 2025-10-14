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
#include "spdlog/spdlog.h"

// Forward declaration to avoid circular dependency
class Engine;
class Registry;

class ComponentManager {
public:
    explicit ComponentManager(Engine& engine, Registry& registry);

    // Entity order callback setup
    void setEntityOrderCallback(std::function<const std::vector<Entity>& ()> callback);

    // Component registration
    template<typename T>
    void registerComponent();

    // Component lifecycle
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args);
    bool addComponentByName(Entity entity, const std::string& componentName, const nlohmann::json& componentData = {});

    template<typename T>
    void removeComponent(Entity entity);
    void removeComponentByName(Entity entity, const std::string& componentName);

    template<typename T>
    T& getComponent(Entity entity);
    Component* getComponentByName(Entity entity, const std::string& componentName);

    template<typename T>
    bool hasComponent(Entity entity) const;

    // Entity queries
    template<typename... Components>
    std::vector<Entity> createView();

    template<typename... Components>
    std::vector<Entity> createOrderedView();

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

    // Order management
    void invalidateAllOrders();

private:
    void initializeComponents();

    // Component pool interface
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
        virtual nlohmann::json serializeComponent(Entity entity) const = 0;
        virtual void deserializeComponent(Entity entity, const nlohmann::json& data) = 0;
        virtual bool hasEntity(Entity entity) const = 0;
        virtual Component* getComponentPtr(Entity entity) = 0;
        virtual void invalidateOrder() = 0;
        virtual std::vector<Entity> getOrderedEntities() = 0;
    };

    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        explicit ComponentPool(Engine& engine, Registry& registry) : m_engine(engine), m_registry(registry) {}

        std::vector<T> m_components;
        std::unordered_map<Entity, size_t> m_entityToIndex;
        std::unordered_map<size_t, Entity> m_indexToEntity;

        // Order management
        mutable std::vector<Entity> m_orderedEntities;
        mutable bool m_orderDirty = true;

        T& add(Entity entity, T&& component);
        void remove(Entity entity) override;

        nlohmann::json serializeComponent(Entity entity) const override;
        void deserializeComponent(Entity entity, const nlohmann::json& data) override;
        bool hasEntity(Entity entity) const override;
        Component* getComponentPtr(Entity entity) override {
            auto it = m_entityToIndex.find(entity);
            if (it != m_entityToIndex.end()) {
                return &m_components[it->second];
            }
            return nullptr;
        }

        void invalidateOrder() override { m_orderDirty = true; }
        std::vector<Entity> getOrderedEntities() override;

    private:
        Engine& m_engine;
        Registry& m_registry;
    };

    using DeserializerFunction = std::function<void(Entity, const nlohmann::json&)>;

    Engine& m_engine;
    Registry& m_registry; // Reference to Registry
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_componentPools;
    std::unordered_map<std::type_index, std::string> m_componentTypes;
    std::unordered_map<std::string, std::type_index> m_nameToType;
    std::unordered_map<std::string, DeserializerFunction> m_deserializerFunctions;

    // Entity order callback
    std::function<const std::vector<Entity>& ()> m_entityOrderCallback;

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
        component.setEngine(&m_engine);
        component.setRegistry(&m_registry);
        component.deserialize(data);
        addComponent<T>(entity, std::move(component));
        };
}

template<typename T, typename... Args>
T& ComponentManager::addComponent(Entity entity, Args&&... args) {
    auto type = std::type_index(typeid(T));
    auto poolIt = m_componentPools.find(type);

    if (poolIt == m_componentPools.end()) {
        auto pool = std::make_unique<ComponentPool<T>>(m_engine, m_registry);
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
std::vector<Entity> ComponentManager::createView() {
    std::vector<Entity> result;

    // Znajdź pool pierwszego komponentu
    auto firstType = std::type_index(typeid(std::tuple_element_t<0, std::tuple<Components...>>));
    auto poolIt = m_componentPools.find(firstType);
    if (poolIt == m_componentPools.end()) return result;

    auto& firstPool = static_cast<ComponentPool<std::tuple_element_t<0, std::tuple<Components...>>>&>(*poolIt->second);

    // Iteruj przez encje w pierwszym poolu
    for (const auto& [entity, index] : firstPool.m_entityToIndex) {
        bool hasAll = (hasComponent<Components>(entity) && ...);
        if (hasAll) {
            result.push_back(entity);
        }
    }

    return result;
}

template<typename... Components>
std::vector<Entity> ComponentManager::createOrderedView() {
    std::vector<Entity> result;

    try {
        const auto& globalOrder = m_entityOrderCallback();

        for (Entity entity : globalOrder) {
            bool hasAll = (hasComponent<Components>(entity) && ...);
            if (hasAll) {
                result.push_back(entity);
            }
        }
    }
    catch (const std::exception& e) {
        // Entity order callback not set or failed - this should not happen in normal operation
        SPDLOG_ERROR("Entity order callback failed: {}", e.what());
        throw std::runtime_error("Entity order callback is not properly initialized");
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
    component.setEngine(&m_engine);

    size_t index = m_components.size();
    m_entityToIndex[entity] = index;
    m_indexToEntity[index] = entity;
    m_components.emplace_back(std::move(component));

    // Invalidate order since we added a new entity
    m_orderDirty = true;

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

    // Invalidate order since we removed an entity
    m_orderDirty = true;
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
        m_components[it->second].setEngine(&m_engine);
    }
    else {
        throw std::runtime_error("Entity not found in component pool for deserialization");
    }
}

template<typename T>
bool ComponentManager::ComponentPool<T>::hasEntity(Entity entity) const {
    return m_entityToIndex.find(entity) != m_entityToIndex.end();
}

template<typename T>
std::vector<Entity> ComponentManager::ComponentPool<T>::getOrderedEntities() {
    if (!m_orderDirty) {
        return m_orderedEntities;
    }

    m_orderedEntities.clear();
    m_orderedEntities.reserve(m_components.size());

    for (const auto& [entity, index] : m_entityToIndex) {
        m_orderedEntities.push_back(entity);
    }

    m_orderDirty = false;
    return m_orderedEntities;
}