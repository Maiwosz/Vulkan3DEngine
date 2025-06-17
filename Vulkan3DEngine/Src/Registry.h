#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <set>
#include "Entity.h"
#include "Component.h"
#include "SystemManager.h"
#include "ComponentManager.h"
#include "HierarchyManager.h"
#include <json.hpp>
#include "AssetLib.h"
#include "PrefabInstanceManager.h"
#include "Serializer.h"

class SystemManager;
class ComponentManager;
class HierarchyManager;
class PrefabInstanceManager;
class Serializer;

class Registry {
public:
    Registry();

    // Manager access
    SystemManager& systems() { return *m_systemManager; }
    ComponentManager& components() { return *m_componentManager; }
    HierarchyManager& hierarchy() { return *m_hierarchyManager; }
    PrefabInstanceManager& prefabInstances() { return *m_prefabInstanceManager; }
    Serializer& serialization() { return *m_serializer; }

    // Entity lifecycle
    Entity create(Entity parent = Entity(0), const std::string& name = "");
    Entity create(const std::string& name) {
        return create(Entity(0), name);
    }
    void destroy(Entity entity);
    bool valid(Entity entity) const;

    // Entity naming
    std::string getEntityName(Entity entity) const;
    void setEntityName(Entity entity, const std::string& name);
    Entity findEntityByName(const std::string& name) const;
    bool entityNameExists(const std::string& name) const;

    // Entity operations
    Entity cloneEntity(Entity sourceEntity, Entity newParent = Entity(0), const std::string& name = "");
    std::vector<std::string> getEntityComponentTypes(Entity entity) const;
    bool hasAnyComponents(Entity entity) const;

    // Component management
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args);

    template<typename T>
    void removeComponent(Entity entity);

    template<typename T>
    T& getComponent(Entity entity);

    template<typename T>
    bool hasComponent(Entity entity) const;

    template<typename... Components>
    std::vector<Entity> createView();

    // Hierarchy helpers
    void setParent(Entity child, Entity parent);
    void removeParent(Entity child);
    Entity getParent(Entity entity) const;
    const std::unordered_set<Entity>& getChildren(Entity entity) const;
    bool hasParent(Entity entity) const;

    // Count entities in hierarchy (including root)
    uint32_t countEntitiesInHierarchy(Entity rootEntity) const;

    // Component pool interface
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
        virtual nlohmann::json serializeComponent(Entity entity) const = 0;
        virtual void deserializeComponent(Entity entity, const nlohmann::json& data, Registry& registry) = 0;
        virtual bool hasEntity(Entity entity) const = 0;
    };

    // Helper method for serializer
    IComponentPool* getComponentPool(const std::string& componentTypeName) const;

private:
    // Entity storage
    std::set<Entity> m_entities;
    std::set<Entity> m_freeEntities;
    uint32_t m_nextId = 1;

    // Entity naming
    std::unordered_map<Entity, std::string> m_entityNames;
    std::unordered_map<std::string, Entity> m_nameToEntity;

    // Managers
    std::unique_ptr<SystemManager> m_systemManager;
    std::unique_ptr<ComponentManager> m_componentManager;
    std::unique_ptr<HierarchyManager> m_hierarchyManager;
    std::unique_ptr<PrefabInstanceManager> m_prefabInstanceManager;
    std::unique_ptr<Serializer> m_serializer;

    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        std::vector<T> m_components;
        std::unordered_map<Entity, size_t> m_entityToIndex;
        std::unordered_map<size_t, Entity> m_indexToEntity;

        T& add(Entity entity, T&& component, Registry& registry);
        void remove(Entity entity) override;

        nlohmann::json serializeComponent(Entity entity) const override;
        void deserializeComponent(Entity entity, const nlohmann::json& data, Registry& registry) override;
        bool hasEntity(Entity entity) const override;
    };

    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_componentPools;

    // Helper methods
    std::string generateUniqueEntityName(const std::string& baseName = "Entity") const;
};

// Template implementations
template<typename T, typename... Args>
T& Registry::addComponent(Entity entity, Args&&... args) {
    auto type = std::type_index(typeid(T));
    auto poolIt = m_componentPools.find(type);

    if (poolIt == m_componentPools.end()) {
        auto pool = std::make_unique<ComponentPool<T>>();
        poolIt = m_componentPools.emplace(type, std::move(pool)).first;
    }

    auto& pool = static_cast<ComponentPool<T>&>(*poolIt->second);
    return pool.add(entity, T(std::forward<Args>(args)...), *this);
}

template<typename T>
void Registry::removeComponent(Entity entity) {
    auto type = std::type_index(typeid(T));
    if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
        it->second->remove(entity);
    }
}

template<typename T>
T& Registry::getComponent(Entity entity) {
    auto type = std::type_index(typeid(T));
    auto& pool = static_cast<ComponentPool<T>&>(*m_componentPools.at(type));
    return pool.m_components[pool.m_entityToIndex.at(entity)];
}

template<typename T>
bool Registry::hasComponent(Entity entity) const {
    auto type = std::type_index(typeid(T));
    if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
        auto& pool = static_cast<const ComponentPool<T>&>(*it->second);
        return pool.m_entityToIndex.count(entity) > 0;
    }
    return false;
}

template<typename... Components>
std::vector<Entity> Registry::createView() {
    std::vector<Entity> result;
    for (auto entity : m_entities) {
        bool hasAll = (hasComponent<Components>(entity) && ...);
        if (hasAll) result.push_back(entity);
    }
    return result;
}

template<typename T>
T& Registry::ComponentPool<T>::add(Entity entity, T&& component, Registry& registry) {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");

    component.entity = entity;
    component.setRegistry(&registry);

    size_t index = m_components.size();
    m_entityToIndex[entity] = index;
    m_indexToEntity[index] = entity;
    m_components.emplace_back(std::move(component));

    return m_components.back();
}

template<typename T>
void Registry::ComponentPool<T>::remove(Entity entity) {
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
nlohmann::json Registry::ComponentPool<T>::serializeComponent(Entity entity) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        throw std::runtime_error("Entity not found in component pool");
    }
    return m_components[it->second].serialize();
}

template<typename T>
void Registry::ComponentPool<T>::deserializeComponent(Entity entity, const nlohmann::json& data, Registry& registry) {
    auto it = m_entityToIndex.find(entity);
    if (it != m_entityToIndex.end()) {
        m_components[it->second].deserialize(data);
    }
    else {
        T component;
        component.deserialize(data);
        add(entity, std::move(component), registry);
    }
}

template<typename T>
bool Registry::ComponentPool<T>::hasEntity(Entity entity) const {
    return m_entityToIndex.find(entity) != m_entityToIndex.end();
}