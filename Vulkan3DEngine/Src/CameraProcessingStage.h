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
    std::shared_ptr<ShaderLib::StructureDefinition> m_directionalLightDef;
    std::shared_ptr<ShaderLib::StructureDefinition> m_pointLightDef;
    std::shared_ptr<ShaderLib::StructureDefinition> m_spotLightDef;

    // Cached reusable instance
    std::shared_ptr<ShaderLib::BufferObjectInstance> m_cachedInstance;

    // Cached field offsets for O(1) direct memory access
    struct GlobalUBOOffsets {
        uint32_t view;
        uint32_t proj;
        uint32_t cameraPosition;
        uint32_t activePointLights;

        // Directional light offsets
        uint32_t dirLight_direction;
        uint32_t dirLight_color;

        // Point lights array base offset and stride
        uint32_t pointLights_base;
        uint32_t pointLights_stride;

        // Point light field offsets (relative to element base)
        uint32_t pointLight_position_rel;
        uint32_t pointLight_radius_rel;
        uint32_t pointLight_color_rel;
    };
    GlobalUBOOffsets m_offsets;

    bool m_firstFrame;
};
