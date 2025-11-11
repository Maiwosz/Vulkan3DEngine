#include "DescriptorSetGuard.h"
#include "LogicalDevice.h"
#include <spdlog/spdlog.h>
#include <algorithm>

// ========== DescriptorSetGuard Implementation ==========

DescriptorSetGuard::DescriptorSetGuard(
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> descriptorSet,
    VkFence fence,
    const LogicalDevice& device
)
    : m_descriptorSet(std::move(descriptorSet)),
    m_fence(fence),
    m_device(&device),
    m_trackingMode(TrackingMode::Fence)
{
    if (!m_descriptorSet.isValid()) {
        SPDLOG_WARN("DescriptorSetGuard created with invalid descriptor set");
    }

    SPDLOG_TRACE("DescriptorSetGuard created (fence-tracked) for descriptor set {}",
        m_descriptorSet.handle().id);
}

DescriptorSetGuard::DescriptorSetGuard(
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> descriptorSet,
    VkSemaphore semaphore
)
    : m_descriptorSet(std::move(descriptorSet)),
    m_semaphore(semaphore),
    m_trackingMode(TrackingMode::Semaphore)
{
    if (!m_descriptorSet.isValid()) {
        SPDLOG_WARN("DescriptorSetGuard created with invalid descriptor set");
    }

    SPDLOG_TRACE("DescriptorSetGuard created (semaphore-tracked) for descriptor set {}",
        m_descriptorSet.handle().id);
}

bool DescriptorSetGuard::isComplete() const {
    if (m_released) {
        return true;
    }

    switch (m_trackingMode) {
    case TrackingMode::Fence:
        if (m_fence != VK_NULL_HANDLE && m_device) {
            VkResult result = vkGetFenceStatus(m_device->get(), m_fence);
            return result == VK_SUCCESS;
        }
        return false;

    case TrackingMode::Semaphore:
        // Semaphores don't have status query in Vulkan
        // Caller needs to track completion externally
        return false;

    case TrackingMode::None:
        return true;
    }

    return false;
}

void DescriptorSetGuard::waitForCompletion() {
    if (m_released) {
        return;
    }

    if (m_trackingMode == TrackingMode::Fence && m_fence != VK_NULL_HANDLE && m_device) {
        VkResult result = vkWaitForFences(m_device->get(), 1, &m_fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            SPDLOG_ERROR("Failed to wait for fence in DescriptorSetGuard");
        }
    }
}

void DescriptorSetGuard::release() {
    if (m_released) {
        return;
    }

    if (!m_descriptorSet.isValid()) {
        m_released = true;
        return;
    }

    SPDLOG_TRACE("Releasing descriptor set {} from GPU tracking",
        m_descriptorSet.handle().id);

    // The SmartHandle will automatically handle reference counting
    // We just need to mark as released
    m_released = true;

    // Descriptor set will be returned to reusable pool when SmartHandle is destroyed
    // or when all references are released
}

// ========== DescriptorSetGuardPool Implementation ==========

void DescriptorSetGuardPool::addGuard(std::unique_ptr<DescriptorSetGuard> guard) {
    if (!guard) {
        SPDLOG_WARN("Attempted to add null guard to pool");
        return;
    }

    m_guards.push_back(std::move(guard));

    SPDLOG_TRACE("Guard added to pool, total active guards: {}", m_guards.size());
}

size_t DescriptorSetGuardPool::pollAndCleanup() {
    size_t releasedCount = 0;

    // Remove completed guards
    auto it = std::remove_if(m_guards.begin(), m_guards.end(),
        [&releasedCount](const std::unique_ptr<DescriptorSetGuard>& guard) {
            if (guard->isComplete()) {
                guard->release();
                releasedCount++;
                return true;
            }
            return false;
        });

    m_guards.erase(it, m_guards.end());

    if (releasedCount > 0) {
        SPDLOG_TRACE("Cleaned up {} completed guards, {} remaining",
            releasedCount, m_guards.size());
    }

    return releasedCount;
}

void DescriptorSetGuardPool::waitAndCleanupAll() {
    SPDLOG_DEBUG("Waiting for all {} guards to complete", m_guards.size());

    for (auto& guard : m_guards) {
        guard->waitForCompletion();
        guard->release();
    }

    m_guards.clear();

    SPDLOG_DEBUG("All guards completed and cleared");
}

void DescriptorSetGuardPool::clear() {
    waitAndCleanupAll();
}
