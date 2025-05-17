#pragma once
#include "System.h"
#include "Registry.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "AssetManager.h"
#include "Engine.h"

class AssetCollectionSystem : public System<> {
public:
    void update(ContextType& context) override;
};