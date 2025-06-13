#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include <string>
#include <unordered_map>
#include <sol/sol.hpp>

struct ScriptComponent : public Component {
public:
    // Set the script path - the file will be loaded when the script is initialized
    void setScript(const std::string& path) {
        m_scriptPath = path;
        incrementVersion();
    }

    const std::string& getScript() const {
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

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["scriptPath"] = m_scriptPath;
        // Note: We don't serialize the Lua state or table as they are runtime-specific
        // and will be recreated when the script is reloaded
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("scriptPath") && j["scriptPath"].is_string()) {
            m_scriptPath = j["scriptPath"];
            // Reset runtime state - script will need to be reinitialized
            m_initialized = false;
            m_luaState = nullptr;
            m_table = nullptr;
            incrementVersion();
        }
    }

    // IBinarySerializable implementation
    std::vector<uint8_t> serializeBinary() const override {
        BinaryWriter writer;

        // Write script path
        writer.write(m_scriptPath);

        // Note: We don't serialize runtime state (initialized flag, lua state, table)
        // as they are runtime-specific and will be recreated when the script is reloaded

        return writer.getData();
    }

    size_t deserializeBinary(const uint8_t* data, size_t size) override {
        BinaryReader reader(data, size);

        if (!reader.read(m_scriptPath)) return 0;

        // Reset runtime state - script will need to be reinitialized
        m_initialized = false;
        m_luaState = nullptr;
        m_table = nullptr;

        incrementVersion();
        return reader.getPosition();
    }

private:
    std::string m_scriptPath;
    bool m_initialized = false;
    sol::state* m_luaState = nullptr;  // Pointer to lua state owned by ScriptSystem
    sol::table* m_table = nullptr;     // Script instance table
};