#pragma once
#include <VMA/vk_mem_alloc.h>

#include <vulkan/vulkan.h>
#include <utility>

class AllocatedResource {
public:
    virtual ~AllocatedResource();

    AllocatedResource() = default;
    AllocatedResource(AllocatedResource&& other) noexcept;
    AllocatedResource& operator=(AllocatedResource&& other) noexcept;

    void* map();
    void unmap();
    bool isMapped() const;
    VkDeviceSize getAllocatedSize() const;

    void copyData(const void* data, VkDeviceSize size);

    void destroy();

protected:
    virtual void destroyResourceImpl() = 0;

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    void* m_mappedData = nullptr;
    VkDeviceSize m_allocatedSize = 0;

private:
    void destroyResource();
};