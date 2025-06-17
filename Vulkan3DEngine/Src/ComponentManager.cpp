#include "ComponentManager.h"
#include "ComponentsRegistry.h"
#include <json.hpp>

void ComponentManager::initializeComponents() {
    // Wywołanie centralnej funkcji rejestracji
    registerAllComponents(*this);
}
