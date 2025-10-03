#pragma once
#include "RenderOrder.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <vulkan/vulkan.h>
#include <vector>

// Forward declarations
class Buffer;
class MeshRenderOrder;
class RenderGraph;

class CameraRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Camera; }

    // Camera-specific global uniform buffer and descriptor set
    SmartHandle<UniformBufferHandle, Buffer> globalUBOHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> globalDescriptorSetHandle;

    // Render graph assignment
    SmartHandle<RenderGraphHandle, RenderGraph> renderGraphHandle;

    // Culled mesh list for this camera
    std::vector<std::shared_ptr<MeshRenderOrder>> culledMeshes;

    // Validation helpers
    bool hasValidGlobalUBO() const { return globalUBOHandle.isValid(); }
    bool hasValidGlobalDescriptorSet() const { return globalDescriptorSetHandle.isValid(); }
    bool hasValidRenderGraph() const { return renderGraphHandle.isValid(); }
    bool hasGlobalData() const { return hasValidGlobalUBO() && hasValidGlobalDescriptorSet(); }

    // Check if camera is ready for rendering
    bool isReadyForRendering() const {
        return hasGlobalData() && hasValidRenderGraph() && !culledMeshes.empty();
    }

    // Get culled mesh count
    size_t getCulledMeshCount() const { return culledMeshes.size(); }

    // Uses default implementation from RenderOrder which logs error
    // Camera orders should not reach the direct execution stage - they are processed by RenderStage
};