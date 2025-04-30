#pragma once
#include "Prerequisites.h"
#include <string>
#include "RenderSystem.h"
#include "Entity.h"
#include "Registry.h"
#include "CameraSystem.h"
#include "LightSystem.h"
#include "AssetCollectionSystem.h"


class Scene {
public:
    Scene();
    ~Scene();

    void update();

private:
	Entity testEntity;
	Entity testDirectionalLight;
	Entity testPointLight;
	Entity testCamera;
	std::unique_ptr<Registry> m_registry;
};

