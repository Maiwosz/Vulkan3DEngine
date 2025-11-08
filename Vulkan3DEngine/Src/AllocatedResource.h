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

    // IBufferMapping implementation
    void* getMappedPointer() override { return m_mappedData; }
    const void* getMappedPointer() const override { return m_mappedData; }
    bool isMapped() const noexcept override { return m_mappedData != nullptr; }
    size_t getAllocatedSize() const noexcept override { return static_cast<size_t>(m_allocatedSize); }

    void* map() override;
    void unmap() override;

    // Legacy VMA-style accessor (kept for compatibility)
    VkDeviceSize getAllocatedSizeVk() const noexcept { return m_allocatedSize; }

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
