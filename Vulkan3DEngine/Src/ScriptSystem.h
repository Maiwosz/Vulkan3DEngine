#pragma once
#include "System.h"
#include "IScriptSystem.h"
#include <memory>
#include <vector>

// Main script system that manages both Lua and C++ script subsystems
class ScriptSystem : public System<> {
public:
    ScriptSystem() = default;
    ~ScriptSystem();

    void initialize() override;
    void update() override;

    // Register a script subsystem (Lua, C++, etc.)
    void registerScriptSubsystem(std::unique_ptr<IScriptSystem> subsystem);

    // Get specific subsystem by name
    IScriptSystem* getSubsystem(const std::string& name);

private:
    std::vector<std::unique_ptr<IScriptSystem>> m_subsystems;
};
