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

    // ¯¹danie bufora o minimalnym rozmiarze, powi¹zanego z p³otkiem
    Buffer requestBuffer(VkDeviceSize size, VkFence fence);

    // Zwolnienie buforów powi¹zanych z sygnalizowanymi p³otkami
    void reclaimBuffers();

    void returnBuffer(Buffer buffer);

private:
    VmaAllocator m_allocator;
    const LogicalDevice& m_device;
    std::mutex m_mutex;
    std::vector<Buffer> m_availableBuffers;       // Buffery gotowe do u¿ycia
    std::unordered_map<VkFence, std::vector<Buffer>> m_inUseBuffers; // Buffery w u¿yciu
};