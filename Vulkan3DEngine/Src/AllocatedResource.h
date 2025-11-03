#pragma once
#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <utility>
#include "IBufferMapping.h"

// Base class for all VMA-allocated resources
class AllocatedResource : public ShaderLib::IBufferMapping {
public:
    virtual ~AllocatedResource() = default;

    // Non-copyable but movable
    AllocatedResource(const AllocatedResource&) = delete;
    AllocatedResource& operator=(const AllocatedResource&) = delete;

    AllocatedResource(AllocatedResource&& other) noexcept;
    AllocatedResource& operator=(AllocatedResource&& other) noexcept;

    // Memory mapping operations - implementacja IBufferMapping
    void* map() override;
    void unmap() override;
    bool isMapped() const noexcept override { return m_mappedData != nullptr; }
    VkDeviceSize getAllocatedSize() const noexcept { return m_allocatedSize; }

    // Utility for copying data
    void copyData(const void* data, VkDeviceSize size);

protected:
    AllocatedResource() = default;

    void initializeAllocation(VmaAllocator allocator, VmaAllocation allocation);
    virtual void destroyResource() = 0;
    void cleanup();
    void reset();

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VkDeviceSize m_allocatedSize = 0;
    void* m_mappedData = nullptr;
};