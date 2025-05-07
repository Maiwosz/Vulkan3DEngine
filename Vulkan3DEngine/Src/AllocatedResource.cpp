#include "AllocatedResource.h"
#include <stdexcept>

AllocatedResource::~AllocatedResource() {
    if (m_allocator != VK_NULL_HANDLE && m_allocation != nullptr) {
        destroyResource();
    }
}

AllocatedResource::AllocatedResource(AllocatedResource&& other) noexcept {
    *this = std::move(other);
}

AllocatedResource& AllocatedResource::operator=(AllocatedResource&& other) noexcept {
    if (this != &other) {
        if (m_allocator && m_allocation) {
            destroyResource();
        }

        m_allocator = other.m_allocator;
        m_allocation = other.m_allocation;
        m_mappedData = other.m_mappedData;
        m_allocatedSize = other.m_allocatedSize;

        other.m_allocator = VK_NULL_HANDLE;
        other.m_allocation = nullptr;
        other.m_mappedData = nullptr;
        other.m_allocatedSize = 0;
    }
    return *this;
}

void AllocatedResource::destroyResource() {
    if (m_allocator != VK_NULL_HANDLE) {
        // Allow derived classes to handle their specific resource cleanup
        destroyResourceImpl();
    }

    // Reset common state
    m_allocation = nullptr;
    m_allocator = VK_NULL_HANDLE;
    m_mappedData = nullptr;
    m_allocatedSize = 0;
}

void* AllocatedResource::map() {
    if (!m_mappedData && m_allocator && m_allocation) {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(m_allocator, m_allocation, &allocInfo);

        VkMemoryPropertyFlags memFlags;
        vmaGetMemoryTypeProperties(m_allocator, allocInfo.memoryType, &memFlags);

        if (!(memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            throw std::runtime_error("Trying to map non-host-visible memory");
        }

        vmaMapMemory(m_allocator, m_allocation, &m_mappedData);
    }
    return m_mappedData;
}

void AllocatedResource::unmap() {
    if (m_mappedData && m_allocator && m_allocation) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mappedData = nullptr;
    }
}

bool AllocatedResource::isMapped() const {
    return m_mappedData != nullptr;
}

VkDeviceSize AllocatedResource::getAllocatedSize() const {
    return m_allocatedSize;
}

void AllocatedResource::copyData(const void* data, VkDeviceSize size)
{
    if (!m_allocator || !m_allocation) {
        throw std::runtime_error("Cannot copy data: resource is not properly initialized");
    }

    if (size > m_allocatedSize) {
        throw std::runtime_error("Cannot copy data: size exceeds allocated memory");
    }

    if (isMapped()) {
        throw std::runtime_error("Cannot copy data: resource is already mapped");
    }

    void* mapped = map();
    if (!mapped) {
        throw std::runtime_error("Failed to map memory for data copy");
    }

    memcpy(mapped, data, size);
    unmap();
}

void AllocatedResource::destroy() {
    destroyResource();
}
