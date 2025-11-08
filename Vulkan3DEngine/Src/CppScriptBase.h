#pragma once
#include "Component.h"
#include <string>

// Base class for all C++ script components
class CppScriptBase : public Component {
public:
    virtual ~CppScriptBase() = default;

    // Script lifecycle - must be implemented by derived scripts
    virtual void OnCreate() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    // Must be implemented to return unique script name
    virtual const char* getScriptName() const = 0;

    // Component interface
    const char* getName() const override final {
        return getScriptName();
    }

    // Check if script has been initialized
    bool isInitialized() const { return m_initialized; }
    void setInitialized(bool initialized) { m_initialized = initialized; }

    // Default serialization (can be overridden)
    json serialize() const override {
        json j;
        j["scriptName"] = getScriptName();
        j["initialized"] = m_initialized;
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("initialized") && j["initialized"].is_boolean()) {
            m_initialized = j["initialized"];
        }
    }

    void renderUI() override {
        ImGui::Text("C++ Script: %s", getScriptName());
        ImGui::Text("Status: %s", m_initialized ? "Initialized" : "Not Initialized");

        if (ImGui::Button("Reinitialize")) {
            m_initialized = false;
        }

        ImGui::Separator();
        renderScriptUI();
    }

    // Override this to add custom UI for your script
    virtual void renderScriptUI() {
        ImGui::TextDisabled("No custom UI");
    }

protected:
    bool m_initialized = false;
};
