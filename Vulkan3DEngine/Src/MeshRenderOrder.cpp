#include "MeshRenderOrder.h"
#include "Renderer.h"
#include "Pipeline.h"
#include "PipelineManager.h"
#include "MeshManager.h"
#include "VramManager.h"
#include "DescriptorAllocator.h"
#include "SwapChain.h"
#include "FrameManager.h"
#include "Buffer.h"
#include "Mesh.h"
#include <spdlog/spdlog.h>
#include "AssetSystem.h"

void MeshRenderOrder::execute(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) {
    SPDLOG_DEBUG("Executing mesh render order");

    // Validate critical resources
    if (!isReadyForRendering()) {
        SPDLOG_ERROR("MeshRenderOrder is not ready for rendering - missing critical resources");
        return;
    }

    try {
        bindPipeline(commandBuffer, renderer);
        setViewportAndScissor(commandBuffer, renderer);
        bindDescriptorSets(commandBuffer, renderer);
        bindVertexAndIndexBuffers(commandBuffer, renderer, assetSystem);
        drawMesh(commandBuffer, renderer, assetSystem);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in MeshRenderOrder::execute: {}", e.what());
    }
}

void MeshRenderOrder::bindPipeline(VkCommandBuffer commandBuffer, Renderer& renderer) {
    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("Invalid pipeline handle in MeshRenderOrder");
        return;
    }

    try {
        Pipeline* pipeline = &renderer.pipelineManager().get(pipelineHandle);
        if (!pipeline) {
            SPDLOG_ERROR("Failed to get pipeline from handle: {}", pipelineHandle.id);
            return;
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
        SPDLOG_DEBUG("Pipeline bound: {}", pipelineHandle.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to bind pipeline: {}", e.what());
    }
}

void MeshRenderOrder::setViewportAndScissor(VkCommandBuffer commandBuffer, Renderer& renderer) {
    VkExtent2D extent = renderer.swapChain().getSwapChainExtent();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    SPDLOG_DEBUG("Viewport and scissor set: {}x{}", extent.width, extent.height);
}

void MeshRenderOrder::bindDescriptorSets(VkCommandBuffer commandBuffer, Renderer& renderer) {
    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("Invalid pipeline handle when binding descriptor sets");
        return;
    }

    try {
        Pipeline* pipeline = &renderer.pipelineManager().get(pipelineHandle);
        VkPipelineLayout pipelineLayout = pipeline->getLayout();

        if (pipelineLayout == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Pipeline layout is null");
            return;
        }

        uint32_t frameIndex = renderer.frameManager().getCurrentFrameIndex();
        auto& descriptorAllocator = renderer.descriptorAllocator();

        // Bind global descriptor set
        if (globalDescriptorSetHandle.isValid()) {
            try {
                VkDescriptorSet globalDescSet = descriptorAllocator.getDescriptorSet(globalDescriptorSetHandle.handle());
                if (globalDescSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 0, 1, &globalDescSet, 0, nullptr);
                    descriptorAllocator.markDescriptorAsUsedByGPU(globalDescriptorSetHandle.handle(), frameIndex);
                    SPDLOG_DEBUG("Global descriptor set bound: {}", globalDescriptorSetHandle.handle().id);
                }
            }
            catch (const std::exception& e) {
                SPDLOG_ERROR("Failed to bind global descriptor set: {}", e.what());
            }
        }

        // Bind object descriptor set
        if (objectDescriptorSetHandle.isValid()) {
            try {
                VkDescriptorSet objectDescSet = descriptorAllocator.getDescriptorSet(objectDescriptorSetHandle.handle());
                if (objectDescSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 1, 1, &objectDescSet, 0, nullptr);
                    descriptorAllocator.markDescriptorAsUsedByGPU(objectDescriptorSetHandle.handle(), frameIndex);
                    SPDLOG_DEBUG("Object descriptor set bound: {}", objectDescriptorSetHandle.handle().id);
                }
            }
            catch (const std::exception& e) {
                SPDLOG_ERROR("Failed to bind object descriptor set: {}", e.what());
            }
        }

        // Bind material descriptor set
        if (materialDescriptorSetHandle.isValid()) {
            try {
                VkDescriptorSet materialDescSet = descriptorAllocator.getDescriptorSet(materialDescriptorSetHandle.handle());
                if (materialDescSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 2, 1, &materialDescSet, 0, nullptr);
                    descriptorAllocator.markDescriptorAsUsedByGPU(materialDescriptorSetHandle.handle(), frameIndex);
                    SPDLOG_DEBUG("Material descriptor set bound: {}", materialDescriptorSetHandle.handle().id);
                }
            }
            catch (const std::exception& e) {
                SPDLOG_ERROR("Failed to bind material descriptor set: {}", e.what());
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in bindDescriptorSets: {}", e.what());
    }
}

void MeshRenderOrder::bindVertexAndIndexBuffers(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) {
    if (!meshHandle.isValid()) {
        SPDLOG_ERROR("Invalid mesh handle in MeshRenderOrder");
        return;
    }

    try {
        // Assuming you have a mesh manager accessible through AssetSystem
        // You'll need to adjust this based on your actual architecture
        const Mesh* mesh = assetSystem.meshManager().getMesh(meshHandle);
        if (!mesh) {
            SPDLOG_ERROR("Mesh not found for handle: {}", meshHandle.id);
            return;
        }

        // Validate mesh buffers
        if (!mesh->vertexBuffer.isValid() || !mesh->indexBuffer.isValid()) {
            SPDLOG_ERROR("Invalid mesh buffers - vertex: {}, index: {}",
                mesh->vertexBuffer.isValid(), mesh->indexBuffer.isValid());
            return;
        }

        // Validate mesh has geometry
        if (mesh->vertexCount == 0 || mesh->indexCount == 0) {
            SPDLOG_WARN("Mesh has no geometry - vertices: {}, indices: {}",
                mesh->vertexCount, mesh->indexCount);
            return;
        }

        // Get vertex buffer
        auto* vertexBufferResource = renderer.vramManager().getResource<Buffer>(mesh->vertexBuffer);
        if (!vertexBufferResource) {
            SPDLOG_ERROR("Vertex buffer resource is null for handle: {}", mesh->vertexBuffer.id);
            return;
        }

        VkBuffer vertexBuffer = vertexBufferResource->get();
        if (vertexBuffer == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Vertex buffer VkBuffer is null");
            return;
        }

        // Get index buffer
        auto* indexBufferResource = renderer.vramManager().getResource<Buffer>(mesh->indexBuffer);
        if (!indexBufferResource) {
            SPDLOG_ERROR("Index buffer resource is null for handle: {}", mesh->indexBuffer.id);
            return;
        }

        VkBuffer indexBuffer = indexBufferResource->get();
        if (indexBuffer == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Index buffer VkBuffer is null");
            return;
        }

        // Bind vertex buffer
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

        // Bind index buffer
        VkIndexType vkIndexType = (mesh->indexType == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, vkIndexType);

        SPDLOG_DEBUG("Vertex and index buffers bound for mesh: {}", meshHandle.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in bindVertexAndIndexBuffers: {}", e.what());
    }
}

void MeshRenderOrder::drawMesh(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) {
    if (!meshHandle.isValid()) {
        SPDLOG_ERROR("Invalid mesh handle in drawMesh");
        return;
    }

    try {
        const Mesh* mesh = assetSystem.meshManager().getMesh(meshHandle);
        if (!mesh) {
            SPDLOG_ERROR("Mesh not found for handle: {}", meshHandle.id);
            return;
        }

        // Validate index buffer size
        size_t indexSize = mesh->getIndexSize();
        uint32_t indexCount = mesh->indexCount;
        VkDeviceSize expectedSize = indexSize * indexCount;
        VkDeviceSize actualSize = renderer.vramManager().getResource<Buffer>(mesh->indexBuffer)->getSize();

        if (expectedSize > actualSize) {
            SPDLOG_ERROR("Index buffer too small: expected {} bytes, got {} bytes",
                expectedSize, actualSize);
            uint32_t safeCount = static_cast<uint32_t>(actualSize / indexSize);
            vkCmdDrawIndexed(commandBuffer, safeCount, 1, 0, 0, 0);
            SPDLOG_WARN("Drawing with reduced index count: {}", safeCount);
        }
        else {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
            SPDLOG_DEBUG("Drawing indexed: count={}", indexCount);
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in drawMesh: {}", e.what());
    }
}