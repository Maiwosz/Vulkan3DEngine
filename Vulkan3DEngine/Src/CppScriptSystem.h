#pragma once
#include "IScriptSystem.h"
#include "CppScriptBase.h"
#include <unordered_map>
#include <set>
#include <functional>
#include <typeindex>
#include "Registry.h"
#include "ComponentManager.h"

class Engine;
class Registry;

class CppScriptSystem : public IScriptSystem {
public:
    CppScriptSystem() = default;
    ~CppScriptSystem();

    void initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;

    bool initializeScript(Entity entity) override;
    void updateScript(Entity entity, float deltaTime) override;
    void destroyScript(Entity entity) override;

    bool isScriptInitialized(Entity entity) const override;
    std::string getScriptSystemName() const override { return "CppScriptSystem"; }

    // Set references (called by ScriptSystem)
    void setEngine(Engine* engine) { m_engine = engine; }
    void setRegistry(Registry* registry) { m_registry = registry; }

    // Register a C++ script type - now handles both ComponentManager and internal registration
    template<typename T>
    void registerScriptType() {
        static_assert(std::is_base_of_v<CppScriptBase, T>,
            "Script must inherit from CppScriptBase");

        auto type = std::type_index(typeid(T));

        // Check if already registered
        if (m_scriptTypes.find(type) != m_scriptTypes.end()) {
            return; // Already registered
        }

        // Get script name
        T temp;
        std::string name = temp.getScriptName();

        // Register in internal tracking - use emplace instead of operator[]
        m_scriptTypes.emplace(type, name);
        m_nameToType.emplace(name, type);

        // Register in ComponentManager (if registry is available)
        if (m_registry) {
            m_registry->components().registerComponent<T>();
        }
    }

    // Check if entity has any C++ script component
    bool hasAnyCppScript(Entity entity) const;

private:
    void callOnCreate(Entity entity, CppScriptBase* script);
    void callOnUpdate(Entity entity, CppScriptBase* script, float deltaTime);
    void callOnDestroy(Entity entity, CppScriptBase* script);

    // Register all C++ scripts - called during initialization
    void registerAllScripts();

    // Track which scripts need OnCreate call
    std::set<Entity> m_createdScripts;

    // Track initialized scripts per entity (multiple scripts per entity possible)
    std::unordered_map<Entity, std::set<std::type_index>> m_initializedScripts;

    // Registry of script types
    std::unordered_map<std::type_index, std::string> m_scriptTypes;
    std::unordered_map<std::string, std::type_index> m_nameToType;

    Engine* m_engine = nullptr;
    Registry* m_registry = nullptr;
};
