#pragma once
#include "VulkanContext.h"
#include "PipelineManager.h"
#include "MaterialManager.h"
#include "CommandBufferManager.h"
#include "Handle.h"
#include "IAssetHandler.h"
#include <vulkan/vulkan.h>
#include <vector>

// Forward declaration
class ShaderManager;

/**
 * ComputeDispatcher - Simple synchronous compute shader execution with Smart Material Handles
 *
 * This class provides a straightforward way to execute compute shaders
 * with blocking execution that waits for completion. Uses Material system
 * for parameter management and descriptor set handling.
 *
 * Key improvements:
 * - Uses SmartAssetHandle for automatic material lifecycle management
 * - Direct access to shader metadata through material's smart shader handle
 * - Automatic workgroup calculation from shader's ComputeShaderInfo
 * - Type-safe material access without manual validity checks
 * - Pipeline caching handled by PipelineManager
 *
 * Usage:
 *   // Get smart handle to compute material
 *   auto computeMat = materialManager.createSmartHandle("myComputeMaterial");
 *   if (!computeMat) {
 *       // Handle error
 *       return;
 *   }
 *
 *   // Set runtime parameters
 *   materialManager.setMaterialParameter(computeMat.handle(), "inputBuffer", inputBufferParam);
 *   materialManager.setMaterialParameter(computeMat.handle(), "outputBuffer", outputBufferParam);
 *
 *   // Dispatch compute work - automatic workgroup calculation
 *   dispatcher.dispatchForDataSize(computeMat, 1024, 1024, 1);
 *
 *   // Or manual dispatch
 *   dispatcher.dispatch(computeMat, 128, 128, 1);
 *
 *   // Work is guaranteed to be complete after dispatch() returns
 */
class ComputeDispatcher {
public:
    ComputeDispatcher(
        VulkanContext& vulkanContext,
        PipelineManager& pipelineManager,
        CommandBufferManager& cmdBufferManager
    );
    ~ComputeDispatcher();

    /**
     * Dispatch compute shader using a Smart Material Handle
     *
     * @param material Smart handle to the compute material
     * @param groupCountX Number of work groups in X dimension
     * @param groupCountY Number of work groups in Y dimension
     * @param groupCountZ Number of work groups in Z dimension
     * @return true if dispatch succeeded, false on error
     */
    bool dispatch(
        const SmartAssetHandle<MaterialHandle, Material>& material,
        uint32_t groupCountX,
        uint32_t groupCountY = 1,
        uint32_t groupCountZ = 1
    );

    /**
     * Dispatch compute shader with push constants
     *
     * @param material Smart handle to the compute material
     * @param pushConstantData Pointer to push constant data
     * @param pushConstantSize Size of push constant data in bytes
     * @param groupCountX Number of work groups in X dimension
     * @param groupCountY Number of work groups in Y dimension
     * @param groupCountZ Number of work groups in Z dimension
     * @return true if dispatch succeeded, false on error
     */
    bool dispatchWithPushConstants(
        const SmartAssetHandle<MaterialHandle, Material>& material,
        const void* pushConstantData,
        uint32_t pushConstantSize,
        uint32_t groupCountX,
        uint32_t groupCountY = 1,
        uint32_t groupCountZ = 1
    );

    /**
     * Execute multiple compute dispatches in sequence
     * Each dispatch will have a pipeline barrier between them
     */
    struct DispatchInfo {
        SmartAssetHandle<MaterialHandle, Material> material;
        uint32_t groupCountX;
        uint32_t groupCountY;
        uint32_t groupCountZ;
    };

    bool dispatchBatch(const std::vector<DispatchInfo>& dispatches);

    /**
     * Convenience method for dispatching with automatic workgroup calculation
     * based on data size and shader's local size from ComputeShaderInfo
     *
     * This method automatically reads the local_size_x/y/z from the shader metadata
     * and calculates the appropriate number of workgroups to cover the data size.
     *
     * @param material Smart handle to the compute material
     * @param dataSizeX Total data elements in X dimension
     * @param dataSizeY Total data elements in Y dimension
     * @param dataSizeZ Total data elements in Z dimension
     * @return true if dispatch succeeded, false on error
     */
    bool dispatchForDataSize(
        const SmartAssetHandle<MaterialHandle, Material>& material,
        uint32_t dataSizeX,
        uint32_t dataSizeY = 1,
        uint32_t dataSizeZ = 1
    );

    /**
     * Validate that a material is suitable for compute dispatch
     * Checks if shader has compute stage and ComputeShaderInfo
     */
    bool isValidComputeMaterial(const SmartAssetHandle<MaterialHandle, Material>& material) const;

private:
    /**
     * Calculate work group counts based on shader's local size and data size
     */
    bool calculateWorkGroups(
        const SmartAssetHandle<ShaderHandle, ShaderAsset>& shader,
        uint32_t dataSizeX,
        uint32_t dataSizeY,
        uint32_t dataSizeZ,
        uint32_t& outGroupsX,
        uint32_t& outGroupsY,
        uint32_t& outGroupsZ
    ) const;

    /**
     * Internal dispatch implementation
     */
    bool dispatchInternal(
        VkCommandBuffer cmdBuffer,
        const SmartAssetHandle<MaterialHandle, Material>& material,
        const void* pushConstantData,
        uint32_t pushConstantSize,
        uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ
    );

    /**
     * Create and begin a one-time compute command buffer
     */
    VkCommandBuffer beginComputeCommands();

    /**
     * End and submit compute command buffer with blocking wait
     */
    void endComputeCommands(VkCommandBuffer cmdBuffer);

    /**
     * Insert compute-to-compute pipeline barrier
     */
    void insertComputeBarrier(VkCommandBuffer cmdBuffer);

    VulkanContext& m_vulkanContext;
    PipelineManager& m_pipelineManager;
    CommandBufferManager& m_cmdBufferManager;

    // Reusable fence for synchronization
    VkFence m_computeFence;
};