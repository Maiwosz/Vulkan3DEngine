#pragma once
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "DescriptorAllocator.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

// Forward declaration
class SynchronizationResourceManager;

/**
 * Guards a descriptor set's lifetime, automatically marking it as used by GPU
 * and releasing it when the associated fence/semaphore signals completion.
 *
 * This is independent of FrameManager and can be used for any GPU operation.
 */
class DescriptorSetGuard {
public:
    // Constructor for fence-based tracking
    DescriptorSetGuard(
        SmartHandle<DescriptorSetHandle, VkDescriptorSet> descriptorSet,
        VkFence fence,
        const LogicalDevice& device
    );

    // Constructor for semaphore-based tracking (requires external fence polling)
    DescriptorSetGuard(
        SmartHandle<DescriptorSetHandle, VkDescriptorSet> descriptorSet,
        VkSemaphore semaphore
    );

    ~DescriptorSetGuard() = default;

    // Check if GPU work is complete (non-blocking)
    bool isComplete() const;

    // Wait for GPU work to complete (blocking)
    void waitForCompletion();

    // Get the descriptor set handle
    DescriptorSetHandle getHandle() const { return m_descriptorSet.handle(); }

    // Get the raw VkDescriptorSet
    VkDescriptorSet get() const { return *m_descriptorSet.get(); }

    // Access to smart handle
    const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& getSmartHandle() const {
        return m_descriptorSet;
    }

    // Release descriptor set from GPU usage tracking
    // Called automatically when guard detects completion
    void release();

    // Check if already released
    bool isReleased() const { return m_released; }

private:
    enum class TrackingMode {
        Fence,
        Semaphore,
        None
    };

    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_descriptorSet;
    VkFence m_fence = VK_NULL_HANDLE;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    const LogicalDevice* m_device = nullptr;
    TrackingMode m_trackingMode = TrackingMode::None;
    bool m_released = false;
};

/**
 * Manages a collection of DescriptorSetGuards, automatically cleaning up
 * completed ones. This should be owned by Renderer or similar high-level class.
 */
class DescriptorSetGuardPool {
public:
    explicit DescriptorSetGuardPool() = default;
    ~DescriptorSetGuardPool() = default;

    // Add a new guard to track
    void addGuard(std::unique_ptr<DescriptorSetGuard> guard);

    // Poll all guards and release completed ones (non-blocking)
    // Returns number of guards released
    size_t pollAndCleanup();

    // Wait for all guards to complete and release them (blocking)
    void waitAndCleanupAll();

    // Get current number of tracked guards
    size_t getActiveGuardCount() const { return m_guards.size(); }

    // Clear all guards (waits for completion first)
    void clear();

private:
    std::vector<std::unique_ptr<DescriptorSetGuard>> m_guards;
};
