#pragma once
#include "AllocatedResource.h"

class Buffer : public AllocatedResource {
public:
    Buffer();
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    static Buffer create(VmaAllocator allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        VkMemoryPropertyFlags requiredFlags = 0);

    static Buffer createStaging(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createVertex(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createIndex(VmaAllocator allocator, VkDeviceSize size);
    static Buffer createUniform(VmaAllocator allocator, VkDeviceSize size);

    VkBuffer get() const { return m_buffer; }
    VkDeviceSize getSize() const { return m_size; }

protected:
    void destroyResourceImpl() override;

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
};