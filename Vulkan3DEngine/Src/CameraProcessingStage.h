#pragma once
#include "ProcessingStage.h"
#include "CameraRenderOrder.h"
#include "LightRenderOrder.h"
#include "Registry.h"
#include "BufferManager.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "BufferObjectDefinition.h"
#include "BuiltInStructures.h"
#include <memory>

// Forward declarations
class Buffer;
class EngineCore;
class CameraComponent;
class TransformComponent;
class LightComponent;

class CameraProcessingStage : public ProcessingStage {
public:
    CameraProcessingStage(ProcessingContext& context, Registry& registry, EngineCore& renderer);
    ~CameraProcessingStage() override = default;

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    ProcessingResult processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder);

    SmartHandle<BufferHandle, Buffer> createGlobalUniformBuffer(
        std::shared_ptr<CameraRenderOrder> cameraOrder,
        const std::vector<std::shared_ptr<LightRenderOrder>>& lights);

    std::vector<std::shared_ptr<LightRenderOrder>> cullLights(
        std::shared_ptr<CameraRenderOrder> cameraOrder,
        const std::vector<std::shared_ptr<LightRenderOrder>>& allLights);

    Registry& m_registry;
    BufferManager& m_bufferManager;

    // Cached definitions
    std::shared_ptr<ShaderLib::BufferObjectDefinition> m_globalUBODef;

    // Cached reusable instance
    std::shared_ptr<ShaderLib::BufferObjectInstance> m_cachedInstance;
};
