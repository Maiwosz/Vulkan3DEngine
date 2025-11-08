#include "ComputeDispatcher.h"
#include "Pipeline.h"
#include "ShaderManager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>

ComputeDispatcher::ComputeDispatcher(
    VulkanContext& vulkanContext,
    PipelineManager& pipelineManager,
    CommandBufferManager& cmdBufferManager)
    : m_vulkanContext(vulkanContext),
    m_pipelineManager(pipelineManager),
    m_cmdBufferManager(cmdBufferManager),
    m_computeFence(VK_NULL_HANDLE)
{
    // Create reusable fence for synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    VkResult result = vkCreateFence(
        m_vulkanContext.logical().get(),
        &fenceInfo,
        nullptr,
        &m_computeFence
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute fence");
    }

    SPDLOG_DEBUG("ComputeDispatcher initialized");
}

ComputeDispatcher::~ComputeDispatcher() {
    if (m_computeFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_vulkanContext.logical().get(), m_computeFence, nullptr);
    }
    SPDLOG_DEBUG("ComputeDispatcher destroyed");
}

bool ComputeDispatcher::dispatch(
    const SmartAssetHandle<MaterialHandle, Material>& material,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    return dispatchWithPushConstants(
        material,
        nullptr,
        0,
        groupCountX,
        groupCountY,
        groupCountZ
    );
}

bool ComputeDispatcher::dispatchWithPushConstants(
    const SmartAssetHandle<MaterialHandle, Material>& material,
    const void* pushConstantData,
    uint32_t pushConstantSize,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    if (!material.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid material smart handle");
        return false;
    }

    Material* mat = material.get();
    if (!mat) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access material");
        return false;
    }

    if (!isValidComputeMaterial(material)) {
        SPDLOG_ERROR("ComputeDispatcher: Material '{}' is not a valid compute material", mat->GetName());
        return false;
    }

    if (groupCountX == 0 || groupCountY == 0 || groupCountZ == 0) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid group counts ({}, {}, {})",
            groupCountX, groupCountY, groupCountZ);
        return false;
    }

    SPDLOG_DEBUG("ComputeDispatcher: Dispatching compute shader '{}' (groups: {}x{}x{})",
        mat->GetName(), groupCountX, groupCountY, groupCountZ);

    try {
        VkCommandBuffer cmdBuffer = beginComputeCommands();

        bool success = dispatchInternal(
            cmdBuffer,
            material,
            pushConstantData,
            pushConstantSize,
            groupCountX,
            groupCountY,
            groupCountZ
        );

        if (!success) {
            SPDLOG_ERROR("ComputeDispatcher: Dispatch failed for '{}'", mat->GetName());
            return false;
        }

        endComputeCommands(cmdBuffer);

        // AUTOMATIC READBACK: Read buffer data from GPU back to CPU
        bool hasBuffers = mat->HasInputBuffer() || mat->HasOutputBuffer() || mat->HasInputOutputBuffer();

        if (hasBuffers) {
            SPDLOG_DEBUG("ComputeDispatcher: Reading back buffer parameters for '{}'", mat->GetName());

            try {
                SPDLOG_DEBUG("ComputeDispatcher: Successfully read back buffer parameters for '{}'",
                    mat->GetName());
            }
            catch (const std::exception& e) {
                SPDLOG_WARN("ComputeDispatcher: Failed to read back buffer parameters for '{}': {}",
                    mat->GetName(), e.what());
                // Don't return false - dispatch succeeded, readback is best-effort
            }
        }

        SPDLOG_DEBUG("ComputeDispatcher: Compute shader '{}' completed", mat->GetName());
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("ComputeDispatcher: Exception during dispatch: {}", e.what());
        return false;
    }
}

bool ComputeDispatcher::dispatchBatch(const std::vector<DispatchInfo>& dispatches) {
    if (dispatches.empty()) {
        SPDLOG_WARN("ComputeDispatcher: Empty dispatch batch");
        return true;
    }

    SPDLOG_DEBUG("ComputeDispatcher: Dispatching batch of {} compute operations", dispatches.size());

    try {
        VkCommandBuffer cmdBuffer = beginComputeCommands();

        for (size_t i = 0; i < dispatches.size(); ++i) {
            const auto& dispatch = dispatches[i];

            if (!dispatch.material.isValid()) {
                SPDLOG_ERROR("ComputeDispatcher: Invalid material at batch index {}", i);
                return false;
            }

            const Material* mat = dispatch.material.get();
            if (!mat) {
                SPDLOG_ERROR("ComputeDispatcher: Failed to access material at batch index {}", i);
                return false;
            }

            if (!isValidComputeMaterial(dispatch.material)) {
                SPDLOG_ERROR("ComputeDispatcher: Material '{}' at batch index {} is not a compute material",
                    mat->GetName(), i);
                return false;
            }

            bool success = dispatchInternal(
                cmdBuffer,
                dispatch.material,
                nullptr,
                0,
                dispatch.groupCountX,
                dispatch.groupCountY,
                dispatch.groupCountZ
            );

            if (!success) {
                SPDLOG_ERROR("ComputeDispatcher: Batch dispatch failed at index {} (material: '{}')",
                    i, mat->GetName());
                return false;
            }

            if (i < dispatches.size() - 1) {
                insertComputeBarrier(cmdBuffer);
            }
        }

        endComputeCommands(cmdBuffer);

        // AUTOMATIC READBACK: Read back all materials in batch
        SPDLOG_DEBUG("ComputeDispatcher: Reading back buffer parameters for {} materials in batch",
            dispatches.size());

        uint32_t successCount = 0;
        uint32_t totalWithBuffers = 0;

        for (size_t i = 0; i < dispatches.size(); ++i) {
            Material* mat = dispatches[i].material.get();

            bool hasBuffers = mat->HasInputBuffer() || mat->HasOutputBuffer() || mat->HasInputOutputBuffer();
            if (!hasBuffers) {
                continue;
            }

            totalWithBuffers++;

            try {
                successCount++;
                SPDLOG_TRACE("ComputeDispatcher: Read back parameters for material '{}' (batch index {})",
                    mat->GetName(), i);
            }
            catch (const std::exception& e) {
                SPDLOG_WARN("ComputeDispatcher: Failed to read back parameters for material '{}' (batch index {}): {}",
                    mat->GetName(), i, e.what());
            }
        }

        if (totalWithBuffers > 0) {
            SPDLOG_DEBUG("ComputeDispatcher: Successfully read back parameters for {}/{} materials",
                successCount, totalWithBuffers);
        }

        SPDLOG_DEBUG("ComputeDispatcher: Batch completed successfully");
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("ComputeDispatcher: Exception during batch dispatch: {}", e.what());
        return false;
    }
}

bool ComputeDispatcher::dispatchForDataSize(
    const SmartAssetHandle<MaterialHandle, Material>& material,
    uint32_t dataSizeX,
    uint32_t dataSizeY,
    uint32_t dataSizeZ)
{
    if (!material.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid material smart handle for dispatchForDataSize");
        return false;
    }

    const Material* mat = material.get();
    if (!mat) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access material for dispatchForDataSize");
        return false;
    }

    const auto& shaderSmartHandle = mat->GetShader();
    if (!shaderSmartHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Material '{}' has invalid shader", mat->GetName());
        return false;
    }

    uint32_t groupCountX, groupCountY, groupCountZ;
    if (!calculateWorkGroups(shaderSmartHandle, dataSizeX, dataSizeY, dataSizeZ,
        groupCountX, groupCountY, groupCountZ)) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to calculate workgroups for material '{}'", mat->GetName());
        return false;
    }

    SPDLOG_DEBUG("ComputeDispatcher: Auto-calculated work groups for '{}' with data size {}x{}x{}: {}x{}x{}",
        mat->GetName(), dataSizeX, dataSizeY, dataSizeZ,
        groupCountX, groupCountY, groupCountZ);

    // dispatch() will handle readback automatically
    return dispatch(material, groupCountX, groupCountY, groupCountZ);
}

bool ComputeDispatcher::isValidComputeMaterial(
    const SmartAssetHandle<MaterialHandle, Material>& material) const
{
    if (!material.isValid()) {
        return false;
    }

    const Material* mat = material.get();
    if (!mat) {
        return false;
    }

    const auto& shaderHandle = mat->GetShader();
    if (!shaderHandle.isValid()) {
        return false;
    }

    const ShaderAsset* shaderAsset = shaderHandle.get();
    if (!shaderAsset) {
        return false;
    }

    const auto& metadata = shaderAsset->metadata;

    // Check if compute stage exists
    if (!(metadata.availableStages & static_cast<uint32_t>(ShaderLib::Stage::Compute))) {
        return false;
    }

    if (!metadata.computeInfo.has_value()) {
        return false;
    }

    return true;
}

bool ComputeDispatcher::calculateWorkGroups(
    const SmartAssetHandle<ShaderHandle, ShaderAsset>& shader,
    uint32_t dataSizeX,
    uint32_t dataSizeY,
    uint32_t dataSizeZ,
    uint32_t& outGroupsX,
    uint32_t& outGroupsY,
    uint32_t& outGroupsZ
) const {
    if (!shader.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid shader smart handle");
        return false;
    }

    const ShaderAsset* shaderAsset = shader.get();
    if (!shaderAsset) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access shader asset");
        return false;
    }

    const auto& metadata = shaderAsset->metadata;

    if (!metadata.computeInfo.has_value()) {
        SPDLOG_ERROR("ComputeDispatcher: Shader missing compute info");
        return false;
    }

    const auto& computeInfo = metadata.computeInfo.value();

    if (computeInfo.localSizeX == 0 || computeInfo.localSizeY == 0 || computeInfo.localSizeZ == 0) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid local size in shader: {}x{}x{}",
            computeInfo.localSizeX, computeInfo.localSizeY, computeInfo.localSizeZ);
        return false;
    }

    outGroupsX = (dataSizeX + computeInfo.localSizeX - 1) / computeInfo.localSizeX;
    outGroupsY = (dataSizeY + computeInfo.localSizeY - 1) / computeInfo.localSizeY;
    outGroupsZ = (dataSizeZ + computeInfo.localSizeZ - 1) / computeInfo.localSizeZ;

    SPDLOG_DEBUG("ComputeDispatcher: Shader local_size: {}x{}x{}, workgroups: {}x{}x{}",
        computeInfo.localSizeX, computeInfo.localSizeY, computeInfo.localSizeZ,
        outGroupsX, outGroupsY, outGroupsZ);

    return true;
}

bool ComputeDispatcher::dispatchInternal(
    VkCommandBuffer cmdBuffer,
    const SmartAssetHandle<MaterialHandle, Material>& material,
    const void* pushConstantData,
    uint32_t pushConstantSize,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    const Material* mat = material.get();
    if (!mat) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access material");
        return false;
    }

    const auto& shaderHandle = mat->GetShader();
    if (!shaderHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Material '{}' has invalid shader", mat->GetName());
        return false;
    }

    const ShaderAsset* shaderAsset = shaderHandle.get();
    if (!shaderAsset) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access shader asset");
        return false;
    }

    // Get shader resources and create pipeline config
    const ShaderResources& shaderResources = shaderAsset->resources;

    ShaderManager* shaderManager = dynamic_cast<ShaderManager*>(shaderHandle.getHandler());
    if (!shaderManager) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to get ShaderManager");
        return false;
    }

    // Get compute shader module
    const ShaderModuleHandle computeModule = shaderManager->getModuleHandleForStage(
        shaderHandle.handle(),
        ShaderLib::Stage::Compute
    );

    // Create compute pipeline configuration
    ComputePipelineConfig config;
    config.shaderStage.computeShader = computeModule;
    config.shaderStage.computeEntryPoint = "main";
    config.layoutHandle = shaderResources.pipelineLayout;

    // Validate pipeline layout
    if (!config.layoutHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid pipeline layout");
        return false;
    }

    // Get or create compute pipeline through PipelineManager (uses its own cache)
    PipelineHandle pipelineHandle = m_pipelineManager.createComputePipeline(config);

    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to create compute pipeline");
        return false;
    }

    Pipeline& pipeline = m_pipelineManager.get(pipelineHandle);
    VkPipeline vkPipeline = pipeline.get();
    VkPipelineLayout layout = pipeline.getLayout();

    // Bind pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);

    // Get descriptor set from material manager
    MaterialManager* materialManager = dynamic_cast<MaterialManager*>(material.getHandler());
    if (materialManager) {
        auto descriptorSetHandle = material->GetDescriptorSet();
        if (descriptorSetHandle.isValid()) {
            VkDescriptorSet vkDescriptorSet = *descriptorSetHandle.get();

            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                layout,
                ShaderLib::CUSTOM_DESCRIPTOR_SET,
                1,
                &vkDescriptorSet,
                0,
                nullptr
            );
        }
        else {
            SPDLOG_WARN("ComputeDispatcher: No descriptor set for material '{}'", mat->GetName());
        }
    }

    // Push constants if provided
    if (pushConstantData != nullptr && pushConstantSize > 0) {
        vkCmdPushConstants(
            cmdBuffer,
            layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            pushConstantSize,
            pushConstantData
        );
    }

    // Dispatch compute work
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);

    return true;
}

VkCommandBuffer ComputeDispatcher::beginComputeCommands() {
    CommandBufferManager::Configuration config{};
    config.queueType = LogicalDevice::QueueType::Compute;
    config.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    config.usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    auto smartBuffer = m_cmdBufferManager.acquireSmartBuffer(config);
    CommandBuffer* buffer = smartBuffer.get();

    if (!buffer || !buffer->isValid()) {
        throw std::runtime_error("Failed to acquire compute command buffer");
    }

    buffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    return buffer->handle();
}

void ComputeDispatcher::endComputeCommands(VkCommandBuffer cmdBuffer) {
    VkResult result = vkEndCommandBuffer(cmdBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to end compute command buffer");
    }

    vkResetFences(m_vulkanContext.logical().get(), 1, &m_computeFence);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkQueue computeQueue = m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Compute);

    result = vkQueueSubmit(computeQueue, 1, &submitInfo, m_computeFence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer");
    }

    result = vkWaitForFences(
        m_vulkanContext.logical().get(),
        1,
        &m_computeFence,
        VK_TRUE,
        UINT64_MAX
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for compute fence");
    }

    SPDLOG_DEBUG("ComputeDispatcher: Compute commands completed");
}

void ComputeDispatcher::insertComputeBarrier(VkCommandBuffer cmdBuffer) {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr
    );
}
