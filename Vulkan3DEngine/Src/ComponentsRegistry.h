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
// To jedyne miejsce gdzie musisz dodawać nowe komponenty
inline void registerAllComponents(ComponentManager& manager) {
    // Komponenty podstawowe
    manager.registerComponent<TransformComponent>();
    manager.registerComponent<MeshComponent>();
    manager.registerComponent<MaterialComponent>();
    manager.registerComponent<LightComponent>();
    manager.registerComponent<CameraComponent>();
    manager.registerComponent<ScriptComponent>();

    // === DODAJ NOWE KOMPONENTY TUTAJ ===
    // manager.registerComponent<TwójNowyComponent>();
}

// === INSTRUKCJA DODAWANIA NOWEGO KOMPONENTU ===
// Żeby dodać nowy komponent:
// 1. Dodaj #include "TwójNowyComponent.h" w sekcji 1
// 2. Dodaj manager.registerComponent<TwójNowyComponent>(); w sekcji 2
// 3. Upewnij się że komponent implementuje getComponentName()
// 4. Gotowe!