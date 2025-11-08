#pragma once
// === SEKCJA 1: INCLUDY WSZYSTKICH KOMPONENTÓW ===
// Dodaj tutaj nowe komponenty
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "LightComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"

// Forward declaration
class ComponentManager;

// === SEKCJA 2: FUNKCJA REJESTRACJI ===
// To jedyne miejsce gdzie musisz dodawać nowe komponenty PODSTAWOWE
// (skrypty C++ są rejestrowane automatycznie przez CppScriptSystem)
inline void registerAllComponents(ComponentManager& manager) {
    // Komponenty podstawowe
    manager.registerComponent<TransformComponent>();
    manager.registerComponent<MeshComponent>();
    manager.registerComponent<MaterialComponent>();
    manager.registerComponent<LightComponent>();
    manager.registerComponent<CameraComponent>();
    manager.registerComponent<ScriptComponent>(); // Lua scripts

    // === DODAJ NOWE KOMPONENTY PODSTAWOWE TUTAJ ===
    // manager.registerComponent<TwójNowyComponent>();

    // UWAGA: NIE dodawaj tutaj skryptów C++!
    // Skrypty C++ są automatycznie rejestrowane przez CppScriptSystem
    // podczas jego inicjalizacji (zobacz CppScriptRegistry.h)
}

// === INSTRUKCJA DODAWANIA NOWEGO KOMPONENTU ===
// Żeby dodać nowy komponent PODSTAWOWY:
// 1. Dodaj #include "TwójNowyComponent.h" w sekcji 1
// 2. Dodaj manager.registerComponent<TwójNowyComponent>(); w sekcji 2
// 3. Upewnij się że komponent implementuje getName()
// 4. Gotowe!

// === INSTRUKCJA DODAWANIA SKRYPTU C++ ===
// Żeby dodać nowy skrypt C++:
// 1. Zobacz CppScriptRegistry.h
// 2. Wszystkie skrypty C++ są automatycznie rejestrowane jako komponenty
//    przez CppScriptSystem podczas inicjalizacji systemów
// 3. NIE dodawaj ich tutaj - ComponentManager powstaje PRZED systemami!
