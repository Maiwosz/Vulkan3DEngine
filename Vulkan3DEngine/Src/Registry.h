#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <set>
#include "Entity.h"
#include "Component.h"
#include "SystemManager.h"

class SystemManager;

class Registry {
public:
    SystemManager& getSystemManager() { return *m_systemManager; }

    Registry();

    Entity create();
    void destroy(Entity entity);
    bool valid(Entity entity) const;

    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args) {
        auto type = std::type_index(typeid(T));
        auto poolIt = m_componentPools.find(type);

        if (poolIt == m_componentPools.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            poolIt = m_componentPools.emplace(type, std::move(pool)).first;
        }

        auto& pool = static_cast<ComponentPool<T>&>(*poolIt->second);
        return pool.add(entity, T(std::forward<Args>(args)...));
    }

    template<typename T>
    void removeComponent(Entity entity) {
        auto type = std::type_index(typeid(T));
        if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
            it->second->remove(entity);
        }
    }

    template<typename T>
    T& getComponent(Entity entity) {
        auto type = std::type_index(typeid(T));
        auto& pool = static_cast<ComponentPool<T>&>(*m_componentPools.at(type));
        return pool.m_components[pool.m_entityToIndex.at(entity)]; // Add m_ prefix
    }

    template<typename T>
    bool hasComponent(Entity entity) const {
        auto type = std::type_index(typeid(T));
        if (auto it = m_componentPools.find(type); it != m_componentPools.end()) {
            auto& pool = static_cast<const ComponentPool<T>&>(*it->second);
            return pool.m_entityToIndex.count(entity) > 0; // Add m_ prefix
        }
        return false;
    }

    template<typename... Components>
    std::vector<Entity> createView() {
        std::vector<Entity> result;
        for (auto entity : m_entities) {
            bool hasAll = (hasComponent<Components>(entity) && ...);
            if (hasAll) result.push_back(entity);
        }
        return result;
    }

private:
    std::set<Entity> m_entities;
    std::set<Entity> m_freeEntities;
    uint32_t m_nextId_ = 1;
    std::unique_ptr<SystemManager> m_systemManager;

    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
    };

    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        std::vector<T> m_components;
        std::unordered_map<Entity, size_t> m_entityToIndex;
        std::unordered_map<size_t, Entity> m_indexToEntity;

        T& add(Entity entity, T&& component) {
            static_assert(std::is_base_of_v<Component, T>,
                "T must inherit from Component");
            component.entity = entity;
            size_t index = m_components.size();
            m_entityToIndex[entity] = index;
            m_indexToEntity[index] = entity;
            m_components.emplace_back(std::move(component));
            return m_components.back();
        }

        void remove(Entity entity) override {
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
    };

    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_componentPools;
};