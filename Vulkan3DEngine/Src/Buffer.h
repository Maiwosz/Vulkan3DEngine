#pragma once
#include "AllocatedResource.h"
#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <utility>

// Buffer class
class Buffer : public AllocatedResource {
public:
    Buffer() = default;
    ~Buffer() { cleanup(); }

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    // Factory methods
    static Buffer create(VmaAllocator allocator, VkDeviceSize size,
        VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
        VkMemoryPropertyFlags requiredFlags = 0);

    static Buffer createStaging(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createVertex(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createIndex(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createUniform(VmaAllocator allocator, VkDeviceSize size);

    // Accessors
    VkBuffer get() const noexcept { return m_buffer; }
    VkDeviceSize getSize() const noexcept { return m_size; }
    VkBufferUsageFlags getUsage() const noexcept { return m_usage; }

    explicit operator bool() const noexcept { return m_buffer != VK_NULL_HANDLE; }

protected:
    void destroyResource() override;

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VkBufferUsageFlags m_usage = 0;
};