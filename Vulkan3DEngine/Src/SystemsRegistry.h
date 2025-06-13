#pragma once
// === SEKCJA 1: INCLUDY WSZYSTKICH SYSTEMÓW ===
// Dodaj tutaj nowe systemy
#include "AssetCollectionSystem.h"
#include "CameraSystem.h"
#include "LightSystem.h"
#include "MeshRenderSystem.h"
#include "ScriptSystem.h"

// Forward declaration
class SystemManager;

// === SEKCJA 2: FUNKCJA REJESTRACJI ===
// To jedyne miejsce gdzie musisz dodawać nowe systemy
inline void registerAllSystems(SystemManager& manager) {
    // Systemy podstawowe - zawsze aktywne
    manager.registerSystemActive<AssetCollectionSystem>();

    // Systemy opcjonalne - domyślnie nieaktywne  
    manager.registerSystem<MeshRenderSystem>();
    manager.registerSystem<CameraSystem>();
    manager.registerSystem<LightSystem>();
    manager.registerSystem<ScriptSystem>();

    // === DODAJ NOWE SYSTEMY TUTAJ ===
    // manager.registerSystem<TwójNowySystem>();
    // manager.registerSystemActive<TwójNowySystemAktywny>();
}

// === INSTRUKCJA DODAWANIA NOWEGO SYSTEMU ===
// Żeby dodać nowy system:
// 1. Dodaj #include "TwójNowySystem.h" w sekcji 1
// 2. Dodaj manager.registerSystem<TwójNowySystem>(); w sekcji 2
// 3. Gotowe!
// 
// Opcje rejestracji:
// - registerSystem<T>() - system nieaktywny na start
// - registerSystemActive<T>() - system aktywny na start
