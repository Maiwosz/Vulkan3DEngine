#pragma once
#include "RenderOrder.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <vulkan/vulkan.h>

// Forward declarations
class Buffer;
class Renderer;

class MeshRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Mesh; }

    // Polymorphic execution for mesh rendering
    void execute(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) override;

    // Asset resolution stage
    MeshHandle meshHandle;
    MaterialHandle materialHandle;

    // Uniform buffer stage - używamy SmartHandle dla automatycznego zarządzania
    SmartHandle<UniformBufferHandle, Buffer> globalUBOHandle;
    SmartHandle<UniformBufferHandle, Buffer> objectUBOHandle;

    // Descriptor sets stage - używamy SmartHandle dla automatycznego zarządzania  
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> globalDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> objectDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> materialDescriptorSetHandle;

    // Pipeline assignment stage
    PipelineHandle pipelineHandle;

    // Metody pomocnicze do sprawdzania ważności zasobów
    bool hasValidGlobalUBO() const { return globalUBOHandle.isValid(); }
    bool hasValidObjectUBO() const { return objectUBOHandle.isValid(); }
    bool hasValidGlobalDescriptorSet() const { return globalDescriptorSetHandle.isValid(); }
    bool hasValidObjectDescriptorSet() const { return objectDescriptorSetHandle.isValid(); }
    bool hasValidMaterialDescriptorSet() const { return materialDescriptorSetHandle.isValid(); }

    // Metoda do sprawdzenia czy wszystkie krytyczne zasoby są dostępne
    bool isReadyForRendering() const {
        return meshHandle.isValid() &&
            materialHandle.isValid() &&
            pipelineHandle.isValid() &&
            hasValidGlobalUBO() &&
            hasValidObjectUBO();
    }

private:
    void bindPipeline(VkCommandBuffer commandBuffer, Renderer& renderer);
    void setViewportAndScissor(VkCommandBuffer commandBuffer, Renderer& renderer);
    void bindDescriptorSets(VkCommandBuffer commandBuffer, Renderer& renderer);
    void bindVertexAndIndexBuffers(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem);
    void drawMesh(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem);
};