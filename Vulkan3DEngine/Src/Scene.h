#pragma once
#include "Prerequisites.h"
#include <string>
#include "Entity.h"
#include "Registry.h"


class Scene {
public:
    Scene(Engine& engine, Registry& registry);
    ~Scene();

    void update();

private:
    Engine& m_engine;
    Registry& m_registry;


    Entity testEntity;
    Entity testEntity2;
    Entity testEntity3;
    Entity testDirectionalLight;
    Entity testPointLight;
    Entity testPointLight2;
    Entity testCamera;
    Entity testFloor;
};