#pragma once
#include "Buffer.h"
#include "LogicalDevice.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <vulkan/vulkan.h>

class StagingBufferManager {
public:
    StagingBufferManager(VmaAllocator allocator, const LogicalDevice& device);
    ~StagingBufferManager();

    StagingBufferManager(const StagingBufferManager&) = delete;
    StagingBufferManager& operator=(const StagingBufferManager&) = delete;

    // Żądanie bufora o minimalnym rozmiarze, powiązanego z płotkiem
    Buffer* requestBuffer(VkDeviceSize size, VkFence fence);

    // Zwolnienie buforów powiązanych z sygnalizowanymi płotkami
    void reclaimBuffers();

private:
    VmaAllocator m_allocator;
    const LogicalDevice& m_device;
    std::mutex m_mutex;
    std::vector<Buffer> m_availableBuffers;       // Buffery gotowe do użycia
    std::unordered_map<VkFence, std::vector<Buffer>> m_inUseBuffers; // Buffery w użyciu
};