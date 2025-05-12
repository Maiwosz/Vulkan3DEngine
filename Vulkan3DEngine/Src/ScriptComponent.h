#pragma once
#include "Component.h"
#include <string>
#include <unordered_map>
#include <sol/sol.hpp>

struct ScriptComponent : public Component {
public:
    // Set the script path - the file will be loaded when the script is initialized
    void setScriptPath(const std::string& path) {
        m_scriptPath = path;
        incrementVersion();
    }

    const std::string& getScriptPath() const {
        return m_scriptPath;
    }

    // Check if the script has been initialized
    bool isInitialized() const {
        return m_initialized;
    }

    // Internal use by ScriptSystem
    void setLuaState(sol::state* state) {
        m_luaState = state;
    }

    sol::state* getLuaState() const {
        return m_luaState;
    }

    void setInitialized(bool initialized) {
        m_initialized = initialized;
    }

    // Store script variables between frames
    template<typename T>
    void setVariable(const std::string& name, const T& value) {
        if (m_luaState && m_table) {
            (*m_table)[name] = value;
        }
    }

    template<typename T>
    T getVariable(const std::string& name, const T& defaultValue = T()) {
        if (m_luaState && m_table && m_table->get<sol::object>(name).valid()) {
            return (*m_table)[name];
        }
        return defaultValue;
    }

    void setTable(sol::table* table) {
        m_table = table;
    }

    sol::table* getTable() const {
        return m_table;
    }

private:
    std::string m_scriptPath;
    bool m_initialized = false;
    sol::state* m_luaState = nullptr;  // Pointer to lua state owned by ScriptSystem
    sol::table* m_table = nullptr;     // Script instance table
};