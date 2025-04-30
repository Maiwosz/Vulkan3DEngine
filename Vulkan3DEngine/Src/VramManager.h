#pragma once
#include "VulkanContext.h"
#include "CommandPool.h"
#include "CommandBuffer.h"
#include "Buffer.h"
#include "Image.h"
#include "VramHandle.h"
#include "FrameManager.h"
#include <VMA/vk_mem_alloc.h>
#include <variant>
#include <unordered_map>
#include "GraphicsTypes.h"
#include "StagingBufferManager.h"

class VramManager {
public:
    using Resource = std::variant<Buffer, Image>;

    explicit VramManager(
        VulkanContext& context,
        FrameManager& frameManager,
        CommandBufferManager& cmdBufferManager,
        SynchronizationResourceManager& syncResourceManager
    );
    ~VramManager();

    static VmaAllocator createVmaAllocator(VulkanContext& context);

    // Queues transfer with frame resources (executed before next frame)
    VramHandle createBuffer(
        const Graphics::BufferCreateInfo& info,
        const void* initialData = nullptr
    );
    // Queues transfer with frame resources (executed before next frame)
    VramHandle createImage(
        const Graphics::ImageCreateInfo& info,
        const void* initialData = nullptr
    );

    // Performs immediate blocking transfer (for frame-critical resources)
    VramHandle createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        const void* initialData = nullptr
    );
    // Performs immediate blocking transfer (for frame-critical resources)
    VramHandle createImage(
        const VkImageCreateInfo& imageInfo,
        VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        const void* initialData = nullptr
    );

    template<typename T>
    T* getResource(VramHandle handle) {
        auto it = m_resources.find(handle.id);
        return (it != m_resources.end()) ? std::get_if<T>(&it->second) : nullptr;
    }

    void freeResource(VramHandle handle);
    uint64_t getResourceSize(VramHandle handle);

    uint64_t getVramUsed() const;
    uint64_t getVramBudget() const;
    float getVramUsagePercentage() const;

    void reclaimStagingBuffers() { m_stagingManager.reclaimBuffers(); }
    VramHandle registerExternalImage(
        VkImage image,
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

private:
    void recordBufferUpload(CommandBuffer& commandBuffer, Buffer& dst, const void* data, VkDeviceSize size);
    void recordImageUpload(CommandBuffer& commandBuffer, Image& dst, const void* data, const VkImageCreateInfo& imageInfo);

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    
    VulkanContext& m_context;
    FrameManager& m_frameManager;
    CommandBufferManager& m_cmdBufferManager;
    SynchronizationResourceManager& m_syncResourceManager;
    std::unordered_map<uint64_t, Resource> m_resources;
    uint64_t m_nextId = 1;
    StagingBufferManager m_stagingManager;
};