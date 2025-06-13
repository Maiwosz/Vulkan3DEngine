#pragma once
#include "Prerequisites.h"
#include <string>
#include "MeshRenderSystem.h"
#include "Entity.h"
#include "Registry.h"
#include "CameraSystem.h"
#include "LightSystem.h"
#include "AssetCollectionSystem.h"
#include "ScriptSystem.h"
#include "Event.h"
#include "InputSystem.h"

class Scene {
public:
    Scene();
    ~Scene();

    void update();
    Registry& registry() { return *m_registry; }

private:
    Entity testEntity;
    Entity testEntity2;
    Entity testEntity3;
    Entity testDirectionalLight;
    Entity testPointLight;
    Entity testPointLight2;
    Entity testCamera;
    Entity testFloor;
    std::unique_ptr<Registry> m_registry;
};