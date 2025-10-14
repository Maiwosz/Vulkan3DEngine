#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <memory>
#include "RenderPassManager.h"

/**
 * Interface for ImGui providers.
 * Allows flexible, optional integration of ImGui into the render graph system.
 *
 * Design principles:
 * - Provider is attached to RenderGraphExecutor
 * - ImGui is always injected into the final render pass
 * - Provider manages its own initialization and reinitialization
 * - Callbacks execute the actual UI code
 */
class IImGuiProvider {
public:
    using ImGuiCallback = std::function<void()>;

    virtual ~IImGuiProvider() = default;

    /**
     * Called once per frame before rendering.
     * Should prepare ImGui for a new frame (e.g., NewFrame()).
     */
    virtual void beginFrame() = 0;

    /**
     * Called once per frame after all UI code but before rendering.
     * Should finalize ImGui frame (e.g., Render()).
     */
    virtual void endFrame() = 0;

    /**
     * Render ImGui draw data to the command buffer.
     * Called by executor within the final render pass.
     *
     * @param commandBuffer Command buffer to record draw commands
     */
    virtual void render(VkCommandBuffer commandBuffer) = 0;

    /**
     * Initialize or reinitialize the provider for a specific render pass.
     * Called when render pass changes or provider is first attached.
     *
     * @param renderPass The render pass to initialize for
     * @param msaaSamples MSAA sample count for this render pass
     * @return true if initialization succeeded
     */
    virtual bool initialize(SmartRenderPassHandle renderPass, VkSampleCountFlagBits msaaSamples) = 0;

    /**
     * Shutdown and cleanup resources.
     */
    virtual void shutdown() = 0;

    /**
     * Check if provider is currently initialized.
     */
    virtual bool isInitialized() const = 0;

    /**
     * Get the render pass this provider is currently initialized for.
     * Returns invalid handle if not initialized.
     */
    virtual SmartRenderPassHandle getCurrentRenderPass() const = 0;

    /**
     * Register a callback to be executed during UI construction.
     * Callbacks are executed in registration order between beginFrame() and endFrame().
     *
     * @param callback Function to call for UI construction
     * @return ID of registered callback (for potential removal)
     */
    virtual uint32_t registerCallback(ImGuiCallback callback) = 0;

    /**
     * Unregister a previously registered callback.
     *
     * @param callbackId ID returned from registerCallback()
     * @return true if callback was found and removed
     */
    virtual bool unregisterCallback(uint32_t callbackId) = 0;

    /**
     * Clear all registered callbacks.
     */
    virtual void clearCallbacks() = 0;

    /**
     * Execute all registered callbacks.
     * Called by executor between beginFrame() and endFrame().
     */
    virtual void executeCallbacks() = 0;

    /**
     * Get the number of registered callbacks.
     */
    virtual size_t getCallbackCount() const = 0;
};