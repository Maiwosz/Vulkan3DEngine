#include "Buffer.h"
#include <stdexcept>
#include <string>

Buffer::Buffer(Buffer&& other) noexcept
    : AllocatedResource(std::move(other))
    , m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
    , m_size(std::exchange(other.m_size, 0))
    , m_usage(std::exchange(other.m_usage, 0)) {
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        AllocatedResource::operator=(std::move(other));
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_size = std::exchange(other.m_size, 0);
        m_usage = std::exchange(other.m_usage, 0);
    }
    return *this;
}

Buffer Buffer::create(VmaAllocator allocator, VkDeviceSize size,
    VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
    VkMemoryPropertyFlags requiredFlags) {
    if (size == 0) {
        throw std::runtime_error("Buffer size cannot be zero");
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.requiredFlags = requiredFlags;

    Buffer buffer;
    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
        &buffer.m_buffer, &buffer.m_allocation, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer: " + std::to_string(result));
    }

    buffer.initializeAllocation(allocator, buffer.m_allocation);
    buffer.m_size = size;
    buffer.m_usage = usage;

    return buffer;
}

Buffer Buffer::createStaging(VmaAllocator allocator, VkDeviceSize size) {
    return create(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

Buffer Buffer::createVertex(VmaAllocator allocator, VkDeviceSize size) {
    return create(allocator, size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
}

Buffer Buffer::createIndex(VmaAllocator allocator, VkDeviceSize size) {
    return create(allocator, size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
}

Buffer Buffer::createUniform(VmaAllocator allocator, VkDeviceSize size) {
    return create(allocator, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void Buffer::destroyResource() {
    if (m_buffer != VK_NULL_HANDLE && m_allocator) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_size = 0;
        m_usage = 0;
    }
}