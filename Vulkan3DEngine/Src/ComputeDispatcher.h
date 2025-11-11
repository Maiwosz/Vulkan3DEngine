#pragma once
#include "VulkanContext.h"
#include "PipelineManager.h"
#include "MaterialManager.h"
#include "CommandBufferManager.h"
#include "SynchronizationResourceManager.h"
#include "DescriptorSetGuard.h"
#include "Handle.h"
#include "IAssetHandler.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declaration
class ShaderManager;

// Define handle type for compute tasks
DEFINE_HANDLE_TYPE(ComputeTaskHandle, uint32_t)

/**
 * ComputeDispatcher - Async compute shader execution with task tracking
 *
 * Features:
 *   - Multiple concurrent dispatches (GPU parallelism)
 *   - Task-based tracking with unique IDs
 *   - Non-blocking status checks
 *   - Resource pooling (CommandBuffers & Fences via managers)
 *   - Automatic cleanup of completed tasks
 *   - Descriptor set lifetime management via guards
 *
 * Usage:
 *   ComputeTaskHandle task = dispatcher.dispatch(material, groupsX, groupsY, groupsZ);
 *   // Later...
 *   if (dispatcher.isTaskComplete(task)) {
 *       // Task finished!
 *   }
 *   // Or in main loop:
 *   dispatcher.pollCompletedTasks();  // Auto-cleanup
 */
    class ComputeDispatcher {
    public:
        ComputeDispatcher(
            VulkanContext& vulkanContext,
            PipelineManager& pipelineManager,
            CommandBufferManager& cmdBufferManager,
            SynchronizationResourceManager& syncManager
        );
        ~ComputeDispatcher();

        /**
         * Dispatch compute work (async, returns immediately)
         * @return Task handle for tracking, or invalid handle on failure
         */
        ComputeTaskHandle dispatch(
            const SmartAssetHandle<MaterialHandle, Material>& material,
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1
        );

        /**
         * Dispatch with automatic workgroup calculation based on data size
         * @return Task handle for tracking, or invalid handle on failure
         */
        ComputeTaskHandle dispatchForDataSize(
            const SmartAssetHandle<MaterialHandle, Material>& material,
            uint32_t dataSizeX,
            uint32_t dataSizeY = 1,
            uint32_t dataSizeZ = 1
        );

        // Task status queries
        bool isTaskComplete(ComputeTaskHandle task) const;
        size_t getActiveTaskCount() const { return m_activeTasks.size(); }

        // Wait for specific task (blocking)
        bool waitForTask(ComputeTaskHandle task);

        // Wait for all tasks (blocking)
        void waitForAll();

        // Poll and cleanup completed tasks (non-blocking, call in main loop)
        void pollCompletedTasks();

        // Validation
        bool isValidComputeMaterial(const SmartAssetHandle<MaterialHandle, Material>& material) const;

    private:
        struct ComputeTask {
            VkFence fence;
            SmartAssetHandle<MaterialHandle, Material> material;  // For debugging/logging
            uint32_t groupCountX, groupCountY, groupCountZ;       // For debugging
            std::unique_ptr<DescriptorSetGuard> descriptorGuard;  // Guards descriptor set lifetime
        };

        bool calculateWorkGroups(
            const SmartAssetHandle<ShaderHandle, ShaderAsset>& shader,
            uint32_t dataSizeX,
            uint32_t dataSizeY,
            uint32_t dataSizeZ,
            uint32_t& outGroupsX,
            uint32_t& outGroupsY,
            uint32_t& outGroupsZ
        ) const;

        bool dispatchInternal(
            VkCommandBuffer cmdBuffer,
            const SmartAssetHandle<MaterialHandle, Material>& material,
            uint32_t groupCountX,
            uint32_t groupCountY,
            uint32_t groupCountZ
        );

        VkCommandBuffer beginComputeCommands();
        void submitAsyncCommands(VkCommandBuffer cmdBuffer, VkFence fence);
        void insertComputeBarrier(VkCommandBuffer cmdBuffer);

        VulkanContext& m_vulkanContext;
        PipelineManager& m_pipelineManager;
        CommandBufferManager& m_cmdBufferManager;
        SynchronizationResourceManager& m_syncManager;

        std::unordered_map<ComputeTaskHandle, ComputeTask> m_activeTasks;
        uint32_t m_nextTaskId;
};
