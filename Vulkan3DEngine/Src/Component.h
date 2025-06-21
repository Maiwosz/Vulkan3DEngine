#pragma once
#include "Entity.h"
#include "ISerializable.h"
#include "imgui.h"

class Engine;
class Registry; // Forward declaration

class Component : public ISerializable {
public:
    virtual ~Component() = default;
    Entity entity;

    uint32_t getVersion() const { return m_version; }

    // Dostęp do registry
    Registry* getRegistry() const { return m_registry; }
	Engine* getEngine() const { return m_engine; }

    void setEngine(Engine* engine) { m_engine = engine; }
    void setRegistry(Registry* registry) { m_registry = registry; }

    // Czysto wirtualna metoda do uzyskania nazwy komponentu
    virtual const char* getName() const = 0;

    // Virtual method for rendering UI in editor
    virtual void renderUI() {
        // Default implementation shows basic component info
        ImGui::Text("Component: %s", getName());
    }

protected:
    void incrementVersion() { m_version++; }

private:
    uint32_t m_version = 0;
    Engine* m_engine = nullptr;
    Registry* m_registry = nullptr;
};