#include "SystemManager.h"
#include "SystemsRegistry.h"
#include <json.hpp>

void SystemManager::initializeSystems() {
    // Wywołanie centralnej funkcji rejestracji
    registerAllSystems(*this);
}

json SystemManager::serialize() const {
    json j;

    // Serializujemy stan aktywności systemów
    for (const auto& [type, active] : m_systemActive) {
        // Używamy hash'a typu jako klucza (nie jest to idealne, ale działa)
        j["systemStates"][std::to_string(type.hash_code())] = active;
    }

    return j;
}

void SystemManager::deserialize(const json& j) {
    if (j.contains("systemStates")) {
        for (const auto& [hashStr, active] : j["systemStates"].items()) {
            size_t hash = std::stoull(hashStr);

            // Znajdź system o odpowiednim hash'u
            for (auto& [type, currentActive] : m_systemActive) {
                if (type.hash_code() == hash) {
                    currentActive = active;
                    break;
                }
            }
        }
    }
}