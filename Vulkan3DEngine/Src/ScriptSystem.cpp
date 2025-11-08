#include "ScriptSystem.h"
#include "LuaScriptSystem.h"
#include "CppScriptSystem.h"
#include <spdlog/spdlog.h>
#include "Engine.h"

ScriptSystem::~ScriptSystem() {
    // Shutdown all subsystems in reverse order
    for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it) {
        (*it)->shutdown();
    }
}

void ScriptSystem::initialize() {
    SPDLOG_INFO("Initializing unified ScriptSystem");

    // Register Lua script subsystem
    auto luaSystem = std::make_unique<LuaScriptSystem>();
    luaSystem->setEngine(&engine());
    luaSystem->setRegistry(&registry());
    registerScriptSubsystem(std::move(luaSystem));

    // Register C++ script subsystem
    auto cppSystem = std::make_unique<CppScriptSystem>();
    cppSystem->setEngine(&engine());
    cppSystem->setRegistry(&registry());
    registerScriptSubsystem(std::move(cppSystem));

    // Initialize all subsystems
    for (auto& subsystem : m_subsystems) {
        subsystem->initialize();
    }

    SPDLOG_INFO("ScriptSystem initialized with {} subsystems", m_subsystems.size());
}

void ScriptSystem::update() {
    float deltaTime = engine().deltaTime();

    // Update all subsystems
    for (auto& subsystem : m_subsystems) {
        subsystem->update(deltaTime);
    }
}

void ScriptSystem::registerScriptSubsystem(std::unique_ptr<IScriptSystem> subsystem) {
    if (subsystem) {
        SPDLOG_DEBUG("Registering script subsystem: {}", subsystem->getScriptSystemName());
        m_subsystems.push_back(std::move(subsystem));
    }
}

IScriptSystem* ScriptSystem::getSubsystem(const std::string& name) {
    for (auto& subsystem : m_subsystems) {
        if (subsystem->getScriptSystemName() == name) {
            return subsystem.get();
        }
    }
    return nullptr;
}
