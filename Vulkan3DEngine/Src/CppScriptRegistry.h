#pragma once
// === SEKCJA 1: INCLUDY WSZYSTKICH SKRYPTÓW C++ ===
// Dodaj tutaj nowe skrypty C++
// #include "ExampleCppScript.h"
// #include "PlayerControllerScript.h"
// #include "RotatorScript.h"
#include "ComputeShaderTestScript.h"

// Forward declaration
class CppScriptSystem;

// === SEKCJA 2: FUNKCJA REJESTRACJI SKRYPTÓW ===
// To jedyne miejsce gdzie musisz dodawać nowe skrypty C++
inline void registerAllCppScripts(CppScriptSystem& scriptSystem) {
    // === DODAJ NOWE SKRYPTY C++ TUTAJ ===
    // Przykład:
    // scriptSystem.registerScriptType<ExampleCppScript>();
    // scriptSystem.registerScriptType<PlayerControllerScript>();
    // scriptSystem.registerScriptType<RotatorScript>();
    scriptSystem.registerScriptType<ComputeShaderTestScript>();
}

// === INSTRUKCJA DODAWANIA NOWEGO SKRYPTU C++ ===
// Żeby dodać nowy skrypt C++:
// 1. Stwórz nowy plik nagłówkowy (np. MojSkrypt.h)
// 2. Utwórz klasę dziedziczącą po CppScriptBase
// 3. Zaimplementuj wymagane metody:
//    - const char* getScriptName() const override
//    - OnCreate(), OnUpdate(float), OnDestroy() (opcjonalne)
// 4. Dodaj #include "MojSkrypt.h" w sekcji 1 powyżej
// 5. Dodaj rejestrację w sekcji 2:
//    scriptSystem.registerScriptType<MojSkrypt>();
// 6. Gotowe! Skrypt pojawi się w edytorze jako komponent
//
// UWAGA: CppScriptSystem automatycznie rejestruje skrypty w ComponentManager
//        podczas swojej inicjalizacji, więc nie musisz robić tego ręcznie!

// === PRZYKŁAD IMPLEMENTACJI SKRYPTU ===
/*
#pragma once
#include "CppScriptBase.h"
#include "TransformComponent.h"
#include <spdlog/spdlog.h>

class RotatorScript : public CppScriptBase {
public:
    const char* getScriptName() const override {
        return "RotatorScript";
    }

    void OnCreate() override {
        SPDLOG_INFO("RotatorScript created for entity {}", entity.id);
        m_rotationSpeed = 90.0f; // 90 degrees per second
    }

    void OnUpdate(float deltaTime) override {
        if (!getRegistry()) return;

        auto& components = getRegistry()->components();
        if (components.hasComponent<TransformComponent>(entity)) {
            auto& transform = components.getComponent<TransformComponent>(entity);

            // Rotate around Y axis
            float rotationDelta = m_rotationSpeed * deltaTime;
            glm::vec3 currentRotation = transform.getRotation();
            currentRotation.y += rotationDelta;
            transform.setRotation(currentRotation);
        }
    }

    void OnDestroy() override {
        SPDLOG_INFO("RotatorScript destroyed for entity {}", entity.id);
    }

    // Custom serialization
    json serialize() const override {
        json j = CppScriptBase::serialize();
        j["rotationSpeed"] = m_rotationSpeed;
        return j;
    }

    void deserialize(const json& j) override {
        CppScriptBase::deserialize(j);
        if (j.contains("rotationSpeed")) {
            m_rotationSpeed = j["rotationSpeed"];
        }
    }

    // Custom UI
    void renderScriptUI() override {
        ImGui::DragFloat("Rotation Speed", &m_rotationSpeed, 1.0f, 0.0f, 360.0f);
    }

private:
    float m_rotationSpeed = 90.0f;
};
*/
