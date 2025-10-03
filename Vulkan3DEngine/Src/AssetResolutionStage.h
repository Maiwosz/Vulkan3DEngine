#pragma once
#include "ProcessingStage.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include <spdlog/spdlog.h>
#include "Handle.h"
#include "AssetSystem.h"
#include "Registry.h"

class AssetSystem;
class AssetManager;
class Registry;
class MeshRenderOrder;

class AssetResolutionStage : public ProcessingStage {
public:
    AssetResolutionStage(ProcessingContext& context, Registry& registry, AssetSystem& assetSystem);

    ~AssetResolutionStage();

    // Returns true if order was processed successfully, false if it should be discarded
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    Registry& m_registry;
    AssetManager& m_assetManager;
	MeshManager& m_meshManager;

    // Setup mesh data in DrawCall using resolved mesh asset
    bool setupDrawCallMeshData(MeshRenderOrder& meshOrder);
};