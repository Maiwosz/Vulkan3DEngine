#pragma once
#include "Entity.h"

class Component {
public:
    virtual ~Component() = default;
    Entity entity;

    uint32_t getVersion() const { return m_version; }

protected:
    void incrementVersion() { m_version++; }

private:
    uint32_t m_version = 0;
};