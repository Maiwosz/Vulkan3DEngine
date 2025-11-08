#pragma once
#include "Entity.h"
#include <string>

// Base interface for all script system implementations
class IScriptSystem {
public:
    virtual ~IScriptSystem() = default;

    virtual void initialize() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void shutdown() = 0;

    // Script lifecycle management
    virtual bool initializeScript(Entity entity) = 0;
    virtual void updateScript(Entity entity, float deltaTime) = 0;
    virtual void destroyScript(Entity entity) = 0;

    // Query methods
    virtual bool isScriptInitialized(Entity entity) const = 0;
    virtual std::string getScriptSystemName() const = 0;
};
