#pragma once
#include "Entity.h"
#include "ISerializable.h"

class Registry; // Forward declaration

class Component : public ISerializable {
public:
    virtual ~Component() = default;
    Entity entity;

    uint32_t getVersion() const { return m_version; }

    // Dostęp do registry
    Registry* getRegistry() const { return m_registry; }

    // Publiczna metoda do ustawiania registry (używana przez Registry)
    void setRegistry(Registry* registry) { m_registry = registry; }

    // Czysto wirtualna metoda do uzyskania nazwy komponentu
    virtual const char* getName() const = 0;

protected:
    void incrementVersion() { m_version++; }

private:
    uint32_t m_version = 0;
    Registry* m_registry = nullptr;
};