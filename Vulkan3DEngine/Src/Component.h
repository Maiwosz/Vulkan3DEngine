#pragma once
#include "Entity.h"
#include "ISerializable.h"
#include "IBinarySerializable.h"

class Registry; // Forward declaration

class Component : public ISerializable, public IBinarySerializable {
public:
    virtual ~Component() = default;
    Entity entity;

    uint32_t getVersion() const { return m_version; }

    // Dostęp do registry
    Registry* getRegistry() const { return m_registry; }

    // Publiczna metoda do ustawiania registry (używana przez Registry)
    void setRegistry(Registry* registry) { m_registry = registry; }

protected:
    void incrementVersion() { m_version++; }

private:
    uint32_t m_version = 0;
    Registry* m_registry = nullptr;
};