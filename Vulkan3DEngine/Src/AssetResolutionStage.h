#pragma once
#include "ProcessingStage.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include <spdlog/spdlog.h>
#include "Handle.h"

class AssetManager;
class Registry;

class AssetResolutionStage : public OrderProcessingStage {
public:
    AssetResolutionStage(Registry& registry, AssetManager& assetManager);

    ~AssetResolutionStage();

    void process(std::shared_ptr<RenderOrder> order) override;

private:
    Registry& m_registry;
    AssetManager& m_assetManager;
};