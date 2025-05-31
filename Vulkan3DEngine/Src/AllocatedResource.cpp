#include "AllocatedResource.h"
#include <stdexcept>
#include <cstring>
#include <spdlog/spdlog.h>

AllocatedResource::AllocatedResource(AllocatedResource&& other) noexcept
    : m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE))
    , m_allocation(std::exchange(other.m_allocation, nullptr))
    , m_allocatedSize(std::exchange(other.m_allocatedSize, 0))
    , m_mappedData(std::exchange(other.m_mappedData, nullptr)) {
}

AllocatedResource& AllocatedResource::operator=(AllocatedResource&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, nullptr);
        m_allocatedSize = std::exchange(other.m_allocatedSize, 0);
        m_mappedData = std::exchange(other.m_mappedData, nullptr);
    }
    return *this;
}

void* AllocatedResource::map() {
    if (!m_mappedData && m_allocator && m_allocation) {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(m_allocator, m_allocation, &allocInfo);

        VkMemoryPropertyFlags memFlags;
        vmaGetMemoryTypeProperties(m_allocator, allocInfo.memoryType, &memFlags);

        if (!(memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            throw std::runtime_error("Cannot map non-host-visible memory");
        }

        if (vmaMapMemory(m_allocator, m_allocation, &m_mappedData) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map memory");
        }
    }
    return m_mappedData;
}

void AllocatedResource::unmap() {
    if (m_mappedData && m_allocator && m_allocation) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mappedData = nullptr;
    }
}

void AllocatedResource::copyData(const void* data, VkDeviceSize size) {
    if (!data || size == 0) return;
    if (size > m_allocatedSize) {
        throw std::runtime_error("Data size exceeds allocated memory");
    }

    bool wasMapped = isMapped();
    void* mapped = map();
    std::memcpy(mapped, data, size);

    if (!wasMapped) {
        unmap();
    }
}

void AllocatedResource::initializeAllocation(VmaAllocator allocator, VmaAllocation allocation) {
    m_allocator = allocator;
    m_allocation = allocation;

    if (allocation) {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator, allocation, &allocInfo);
        m_allocatedSize = allocInfo.size;
    }
}

void AllocatedResource::cleanup() {
    if (m_allocator && m_allocation) {
        unmap();

        // Additional safety check - verify allocator is still valid
        // This helps catch cases where allocator was destroyed before resource cleanup
        try {
            destroyResource();
        }
        catch (const std::exception& e) {
            // Log the error but don't rethrow to avoid double-destruction issues
            // This can happen if VMA allocator was destroyed before individual resources
            SPDLOG_ERROR("Error during resource cleanup (allocator may be invalid): {}", e.what());
        }
    }
    reset();
}

void AllocatedResource::reset() {
    m_allocator = VK_NULL_HANDLE;
    m_allocation = nullptr;
    m_allocatedSize = 0;
    m_mappedData = nullptr;
}