#pragma once
#include "ProcessingStage.h"
#include "CameraRenderOrder.h"
#include "LightRenderOrder.h"
#include "Registry.h"
#include "BufferManager.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
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

    // Process camera render orders - returns true on success, false on failure
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    // Process camera-specific logic - returns true on success, false on failure
    ProcessingResult processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder);

    // Create global uniform buffer for this camera with light data
    SmartHandle<BufferHandle, Buffer> createGlobalUniformBuffer(
        std::shared_ptr<CameraRenderOrder> cameraOrder,
        const std::vector<std::shared_ptr<LightRenderOrder>>& lights);

    // Perform light culling for this camera (currently pass-through)
    std::vector<std::shared_ptr<LightRenderOrder>> cullLights(
        std::shared_ptr<CameraRenderOrder> cameraOrder,
        const std::vector<std::shared_ptr<LightRenderOrder>>& allLights);

    Registry& m_registry;
    BufferManager& m_bufferManager;
};