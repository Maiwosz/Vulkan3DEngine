#include "ComponentManager.h"
#include "ComponentsRegistry.h"
#include "Registry.h"
#include "Engine.h"
#include <json.hpp>
#include <stdexcept>
#include <spdlog/spdlog.h>

ComponentManager::ComponentManager(Engine& engine, Registry& registry) : m_engine(engine), m_registry(registry) {
    initializeComponents();
}

bool ComponentManager::addComponentByName(Entity entity, const std::string& componentName, const nlohmann::json& componentData) {
    // Sprawdź czy komponent już istnieje
    if (auto* pool = getComponentPool(componentName); pool && pool->hasEntity(entity)) {
        return false; // Komponent już istnieje
    }

    return createComponentFromData(entity, componentName, componentData);
}

void ComponentManager::removeComponentByName(Entity entity, const std::string& componentName) {
    try {
        std::type_index componentType = getComponentType(componentName);
        auto it = m_componentPools.find(componentType);
        if (it != m_componentPools.end()) {
            it->second->remove(entity);
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to remove component: " + std::string(e.what()));
    }
}

Component* ComponentManager::getComponentByName(Entity entity, const std::string& componentName) {
    auto* pool = getComponentPool(componentName);
    if (!pool || !pool->hasEntity(entity)) {
        return nullptr;
    }

    return pool->getComponentPtr(entity);
}

void ComponentManager::initializeComponents() {
    // Call central registration function
    registerAllComponents(*this);
}

// Entity component introspection
std::vector<std::string> ComponentManager::getEntityComponentTypes(Entity entity) const {
    std::vector<std::string> types;

    for (const auto& [typeIndex, pool] : m_componentPools) {
        if (pool->hasEntity(entity)) {
            try {
                std::string typeName = getComponentName(typeIndex);
                types.push_back(typeName);
            }
            catch (const std::exception& e) {
                SPDLOG_WARN("Failed to get component type name: {}", e.what());
            }
        }
    }

    return types;
}

bool ComponentManager::hasAnyComponents(Entity entity) const {
    for (const auto& [typeIndex, pool] : m_componentPools) {
        if (pool->hasEntity(entity)) {
            return true;
        }
    }
    return false;
}

// Component removal for entity destruction
void ComponentManager::removeAllComponents(Entity entity) {
    for (auto& [typeIndex, pool] : m_componentPools) {
        pool->remove(entity);
    }
}

// Serialization support
nlohmann::json ComponentManager::serializeComponent(Entity entity, const std::string& componentTypeName) const {
    auto* pool = getComponentPool(componentTypeName);
    if (!pool) {
        throw std::runtime_error("Component type not found: " + componentTypeName);
    }
    return pool->serializeComponent(entity);
}

void ComponentManager::deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData) {
    // Check if component already exists on entity
    auto* pool = getComponentPool(componentTypeName);
    if (pool && pool->hasEntity(entity)) {
        // Component exists - update existing
        pool->deserializeComponent(entity, componentData);
        return;
    }

    // Component doesn't exist - create new
    if (!createComponentFromData(entity, componentTypeName, componentData)) {
        SPDLOG_ERROR("Failed to deserialize component '{}' for entity {}", componentTypeName, entity.id);
        throw std::runtime_error("Failed to create component: " + componentTypeName);
    }
}

// Component factory (for deserialization)
bool ComponentManager::createComponentFromData(Entity entity, const std::string& componentName, const nlohmann::json& data) {
    auto it = m_deserializerFunctions.find(componentName);
    if (it != m_deserializerFunctions.end()) {
        try {
            it->second(entity, data);
            return true;
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Failed to create component '{}': {}", componentName, e.what());
            return false;
        }
    }

    SPDLOG_WARN("Unknown component type: {}", componentName);
    return false;
}

// Component registry queries
bool ComponentManager::isComponentRegistered(const std::string& name) const {
    return m_nameToType.find(name) != m_nameToType.end();
}

std::string ComponentManager::getComponentName(std::type_index type) const {
    auto it = m_componentTypes.find(type);
    if (it != m_componentTypes.end()) {
        return it->second;
    }
    throw std::runtime_error("Component type not registered");
}

std::type_index ComponentManager::getComponentType(const std::string& name) const {
    auto it = m_nameToType.find(name);
    if (it != m_nameToType.end()) {
        return it->second;
    }
    throw std::runtime_error("Component name not found: " + name);
}

std::vector<std::string> ComponentManager::getAllComponentNames() const {
    std::vector<std::string> names;
    names.reserve(m_componentTypes.size());

    for (const auto& [typeIndex, name] : m_componentTypes) {
        names.push_back(name);
    }

    return names;
}

size_t ComponentManager::getComponentCount() const {
    return m_componentTypes.size();
}

// Component pool interface helper
ComponentManager::IComponentPool* ComponentManager::getComponentPool(const std::string& componentTypeName) const {
    try {
        std::type_index typeIndex = getComponentType(componentTypeName);
        auto it = m_componentPools.find(typeIndex);
        return (it != m_componentPools.end()) ? it->second.get() : nullptr;
    }
    catch (const std::exception&) {
        return nullptr;
    }
}