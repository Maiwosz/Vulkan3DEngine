#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include "Paths.h"
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <sol/sol.hpp>

struct ScriptComponent : public Component {
public:
    const char* getName() const override {
        return "ScriptComponent";
    }

    // Set the script name - the file will be loaded when the script is initialized
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

    void renderUI() override {
        ImGui::Text("Script Component");

        // Display current script info
        if (!m_scriptPath.empty()) {
            ImGui::Text("Script: %s", m_scriptPath.c_str());
        }
        else {
            ImGui::TextDisabled("No script assigned");
        }

        // Get available script files
        std::vector<std::string> scriptFiles = getAvailableScriptFiles();

        if (scriptFiles.empty()) {
            ImGui::TextDisabled("No script files found in scripts directory");
        }
        else {
            // Find current selection index
            int currentSelection = -1;
            for (int i = 0; i < scriptFiles.size(); i++) {
                if (scriptFiles[i] == m_scriptPath) {
                    currentSelection = i;
                    break;
                }
            }

            // Create combo items
            std::vector<const char*> items;
            items.push_back("None"); // First option for no selection
            for (const auto& file : scriptFiles) {
                items.push_back(file.c_str());
            }

            int comboSelection = currentSelection + 1; // +1 because "None" is at index 0

            if (ImGui::Combo("Script File", &comboSelection, items.data(), items.size())) {
                if (comboSelection == 0) {
                    // "None" selected
                    setScript("");
                }
                else {
                    // Script file selected
                    setScript(scriptFiles[comboSelection - 1]);
                }
            }
        }

        // Manual input fallback
        ImGui::Separator();
        ImGui::Text("Manual Input:");

        char pathBuffer[256];
        size_t copyLen = std::min(m_scriptPath.length(), sizeof(pathBuffer) - 1);
        m_scriptPath.copy(pathBuffer, copyLen);
        pathBuffer[copyLen] = '\0';

        if (ImGui::InputText("Script Name (without extension)", pathBuffer, sizeof(pathBuffer))) {
            setScript(std::string(pathBuffer));
        }

        // Script status
        ImGui::Separator();
        ImGui::Text("Status: %s", m_initialized ? "Initialized" : "Not Initialized");

        if (m_luaState) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Lua State: Active");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Lua State: Inactive");
        }

        // Reinitialize button (for development)
        if (ImGui::Button("Reinitialize Script")) {
            m_initialized = false;
            // The ScriptSystem will handle reinitialization
        }
    }

private:
    std::string m_scriptPath;
    bool m_initialized = false;
    sol::state* m_luaState = nullptr;  // Pointer to lua state owned by ScriptSystem
    sol::table* m_table = nullptr;     // Script instance table

    std::vector<std::string> getAvailableScriptFiles() {
        std::vector<std::string> files;

        try {
            std::string scriptsDir = SCRIPTS_DIR;

            if (std::filesystem::exists(scriptsDir) && std::filesystem::is_directory(scriptsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(scriptsDir)) {
                    if (entry.is_regular_file()) {
                        std::string extension = entry.path().extension().string();
                        // Only include Lua script files
                        if (extension == ".lua") {
                            std::string filename = entry.path().stem().string(); // Get filename without extension
                            files.push_back(filename);
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            // Handle filesystem errors silently in UI context
        }

        std::sort(files.begin(), files.end());
        return files;
    }
};