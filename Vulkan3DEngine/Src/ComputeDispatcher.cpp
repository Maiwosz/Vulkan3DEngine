#include "ComputeDispatcher.h"
#include "Pipeline.h"
#include "ShaderManager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cmath>

ComputeDispatcher::ComputeDispatcher(
    VulkanContext& vulkanContext,
    PipelineManager& pipelineManager,
    CommandBufferManager& cmdBufferManager,
    SynchronizationResourceManager& syncManager)
    : m_vulkanContext(vulkanContext),
    m_pipelineManager(pipelineManager),
    m_cmdBufferManager(cmdBufferManager),
    m_syncManager(syncManager),
    m_nextTaskId(1)  // Start from 1, 0 is reserved for errors
{
    SPDLOG_DEBUG("ComputeDispatcher initialized (async, multi-task support with descriptor guards)");
}

ComputeDispatcher::~ComputeDispatcher() {
    if (!m_activeTasks.empty()) {
        SPDLOG_WARN("ComputeDispatcher: {} pending tasks on destruction, waiting...",
            m_activeTasks.size());
        waitForAll();
    }

    SPDLOG_DEBUG("ComputeDispatcher destroyed");
}

ComputeTaskHandle ComputeDispatcher::dispatch(
    const SmartAssetHandle<MaterialHandle, Material>& material,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    // Validation
    if (!material.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid material smart handle");
        return ComputeTaskHandle{};
    }

    Material* mat = material.get();
    if (!mat) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access material");
        return ComputeTaskHandle{};
    }

    if (!isValidComputeMaterial(material)) {
        SPDLOG_ERROR("ComputeDispatcher: Material '{}' is not a valid compute material",
            mat->GetName());
        return ComputeTaskHandle{};
    }

    if (groupCountX == 0 || groupCountY == 0 || groupCountZ == 0) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid group counts ({}, {}, {})",
            groupCountX, groupCountY, groupCountZ);
        return ComputeTaskHandle{};
    }

    // Generate task handle
    ComputeTaskHandle taskHandle{ m_nextTaskId++ };

    SPDLOG_DEBUG("ComputeDispatcher: Dispatching task {} '{}' (groups: {}x{}x{})",
        taskHandle.id, mat->GetName(), groupCountX, groupCountY, groupCountZ);

    try {
        // Acquire resources
        VkCommandBuffer cmdBuffer = beginComputeCommands();
        VkFence fence = m_syncManager.acquireFence(false);

        // Declare descriptor guard
        std::unique_ptr<DescriptorSetGuard> descriptorGuard;

        // Record commands
        bool success = dispatchInternal(cmdBuffer, material, groupCountX, groupCountY, groupCountZ);

        if (!success) {
            SPDLOG_ERROR("ComputeDispatcher: Failed to record commands for task {}", taskHandle.id);
            m_syncManager.releaseFence(fence);
            return ComputeTaskHandle{};
        }

        // Create descriptor guard now that we have the fence
        auto descriptorSetHandle = material->GetDescriptorSet();
        if (descriptorSetHandle.isValid()) {
            descriptorGuard = std::make_unique<DescriptorSetGuard>(
                descriptorSetHandle,
                fence,
                m_vulkanContext.logical()
            );
            SPDLOG_TRACE("ComputeDispatcher: Created descriptor guard for task {}", taskHandle.id);
        }

        // Submit
        submitAsyncCommands(cmdBuffer, fence);

        // Track task with its descriptor guard
        m_activeTasks[taskHandle] = ComputeTask{
            fence,
            material,
            groupCountX,
            groupCountY,
            groupCountZ,
            std::move(descriptorGuard)  // Transfer ownership to task
        };

        SPDLOG_DEBUG("ComputeDispatcher: Task {} submitted successfully", taskHandle.id);
        return taskHandle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("ComputeDispatcher: Exception during dispatch: {}", e.what());
        return ComputeTaskHandle{};
    }
}

ComputeTaskHandle ComputeDispatcher::dispatchForDataSize(
    const SmartAssetHandle<MaterialHandle, Material>& material,
    uint32_t dataSizeX,
    uint32_t dataSizeY,
    uint32_t dataSizeZ)
{
    if (!material.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid material smart handle");
        return ComputeTaskHandle{};
    }

    const Material* mat = material.get();
    if (!mat) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to access material");
        return ComputeTaskHandle{};
    }

    const auto& shaderSmartHandle = mat->GetShader();
    if (!shaderSmartHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Material '{}' has invalid shader", mat->GetName());
        return ComputeTaskHandle{};
    }

    uint32_t groupCountX, groupCountY, groupCountZ;
    if (!calculateWorkGroups(shaderSmartHandle, dataSizeX, dataSizeY, dataSizeZ,
        groupCountX, groupCountY, groupCountZ)) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to calculate workgroups for '{}'",
            mat->GetName());
        return ComputeTaskHandle{};
    }

    SPDLOG_DEBUG("ComputeDispatcher: Auto-calculated work groups for '{}': {}x{}x{} "
        "(data size: {}x{}x{})",
        mat->GetName(), groupCountX, groupCountY, groupCountZ,
        dataSizeX, dataSizeY, dataSizeZ);

    return dispatch(material, groupCountX, groupCountY, groupCountZ);
}

bool ComputeDispatcher::isTaskComplete(ComputeTaskHandle task) const {
    if (!task.isValid()) return false;

    auto it = m_activeTasks.find(task);
    if (it == m_activeTasks.end()) {
        // Task not found - either invalid or already completed
        return false;
    }

    VkResult result = vkGetFenceStatus(m_vulkanContext.logical().get(), it->second.fence);
    return result == VK_SUCCESS;
}

bool ComputeDispatcher::waitForTask(ComputeTaskHandle task) {
    if (!task.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid task handle");
        return false;
    }

    auto it = m_activeTasks.find(task);
    if (it == m_activeTasks.end()) {
        SPDLOG_WARN("ComputeDispatcher: Task {} not found (already completed or invalid)", task.id);
        return false;
    }

    SPDLOG_DEBUG("ComputeDispatcher: Waiting for task {} to complete", task.id);

    VkResult result = vkWaitForFences(
        m_vulkanContext.logical().get(),
        1,
        &it->second.fence,
        VK_TRUE,
        UINT64_MAX
    );

    if (result != VK_SUCCESS) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to wait for task {}", task.id);
        return false;
    }

    // Release descriptor guard (GPU is done with descriptor set)
    if (it->second.descriptorGuard) {
        it->second.descriptorGuard->release();
    }

    // Return fence to pool and remove task
    m_syncManager.releaseFence(it->second.fence);
    m_activeTasks.erase(it);

    SPDLOG_DEBUG("ComputeDispatcher: Task {} completed", task.id);
    return true;
}

void ComputeDispatcher::waitForAll() {
    if (m_activeTasks.empty()) {
        return;
    }

    SPDLOG_DEBUG("ComputeDispatcher: Waiting for {} tasks to complete", m_activeTasks.size());

    // Collect all fences
    std::vector<VkFence> fences;
    fences.reserve(m_activeTasks.size());
    for (const auto& [taskId, task] : m_activeTasks) {
        fences.push_back(task.fence);
    }

    // Wait for all
    VkResult result = vkWaitForFences(
        m_vulkanContext.logical().get(),
        static_cast<uint32_t>(fences.size()),
        fences.data(),
        VK_TRUE,
        UINT64_MAX
    );

    if (result != VK_SUCCESS) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to wait for all tasks");
    }

    // Release all descriptor guards and return fences
    for (const auto& [taskHandle, task] : m_activeTasks) {
        if (task.descriptorGuard) {
            task.descriptorGuard->release();
        }
        m_syncManager.releaseFence(task.fence);
    }
    m_activeTasks.clear();

    SPDLOG_DEBUG("ComputeDispatcher: All tasks completed");
}

void ComputeDispatcher::pollCompletedTasks() {
    if (m_activeTasks.empty()) {
        return;
    }

    // Check each task and cleanup completed ones
    for (auto it = m_activeTasks.begin(); it != m_activeTasks.end();) {
        VkResult result = vkGetFenceStatus(m_vulkanContext.logical().get(), it->second.fence);

        if (result == VK_SUCCESS) {
            SPDLOG_DEBUG("ComputeDispatcher: Task {} completed (auto-cleanup)", it->first.id);

            // Release descriptor guard
            if (it->second.descriptorGuard) {
                it->second.descriptorGuard->release();
            }

            // Return fence to pool
            m_syncManager.releaseFence(it->second.fence);
            it = m_activeTasks.erase(it);
        }
        else if (result == VK_NOT_READY) {
            // Still running, continue to next
            ++it;
        }
        else {
            // Error
            SPDLOG_ERROR("ComputeDispatcher: Error checking task {} status", it->first.id);

            // Clean up on error
            if (it->second.descriptorGuard) {
                it->second.descriptorGuard->release();
            }
            m_syncManager.releaseFence(it->second.fence);
            it = m_activeTasks.erase(it);
        }
    }
}

bool ComputeDispatcher::isValidComputeMaterial(
    const SmartAssetHandle<MaterialHandle, Material>& material) const
{
    if (!material.isValid()) return false;
    const Material* mat = material.get();
    if (!mat) return false;
    const auto& shaderHandle = mat->GetShader();
    if (!shaderHandle.isValid()) return false;
    const ShaderAsset* shaderAsset = shaderHandle.get();
    if (!shaderAsset) return false;
    const auto& metadata = shaderAsset->metadata;
    if (!(metadata.availableStages & static_cast<uint32_t>(ShaderLib::Stage::Compute))) return false;
    if (!metadata.computeInfo.has_value()) return false;
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
    if (!shader.isValid()) return false;
    const ShaderAsset* shaderAsset = shader.get();
    if (!shaderAsset) return false;
    const auto& metadata = shaderAsset->metadata;
    if (!metadata.computeInfo.has_value()) return false;
    const auto& computeInfo = metadata.computeInfo.value();
    if (computeInfo.localSizeX == 0 || computeInfo.localSizeY == 0 || computeInfo.localSizeZ == 0) return false;

    outGroupsX = (dataSizeX + computeInfo.localSizeX - 1) / computeInfo.localSizeX;
    outGroupsY = (dataSizeY + computeInfo.localSizeY - 1) / computeInfo.localSizeY;
    outGroupsZ = (dataSizeZ + computeInfo.localSizeZ - 1) / computeInfo.localSizeZ;

    return true;
}

bool ComputeDispatcher::dispatchInternal(
    VkCommandBuffer cmdBuffer,
    const SmartAssetHandle<MaterialHandle, Material>& material,
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

    const ShaderResources& shaderResources = shaderAsset->resources;

    ShaderManager* shaderManager = dynamic_cast<ShaderManager*>(shaderHandle.getHandler());
    if (!shaderManager) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to get ShaderManager");
        return false;
    }

    const ShaderModuleHandle computeModule = shaderManager->getModuleHandleForStage(
        shaderHandle.handle(),
        ShaderLib::Stage::Compute
    );

    ComputePipelineConfig config;
    config.shaderStage.computeShader = computeModule;
    config.shaderStage.computeEntryPoint = "main";
    config.layoutHandle = shaderResources.pipelineLayout;

    if (!config.layoutHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Invalid pipeline layout");
        return false;
    }

    PipelineHandle pipelineHandle = m_pipelineManager.createComputePipeline(config);

    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("ComputeDispatcher: Failed to create compute pipeline");
        return false;
    }

    Pipeline& pipeline = m_pipelineManager.get(pipelineHandle);
    VkPipeline vkPipeline = pipeline.get();
    VkPipelineLayout layout = pipeline.getLayout();

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);

    // Bind descriptor sets if available
    MaterialManager* materialManager = dynamic_cast<MaterialManager*>(material.getHandler());
    if (materialManager) {
        auto descriptorSetSmartHandle = material->GetDescriptorSet();
        if (descriptorSetSmartHandle.isValid()) {
            VkDescriptorSet vkDescriptorSet = *descriptorSetSmartHandle.get();

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

            // Note: Guard will be created after this function returns,
            // once we have the fence available
        }
    }

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

void ComputeDispatcher::submitAsyncCommands(VkCommandBuffer cmdBuffer, VkFence fence) {
    VkResult result = vkEndCommandBuffer(cmdBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to end compute command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkQueue computeQueue = m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Compute);

    result = vkQueueSubmit(computeQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer");
    }

    SPDLOG_DEBUG("ComputeDispatcher: Commands submitted (non-blocking)");
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
