#pragma once
#include "VulkanContext.h"
#include "CommandPool.h"
#include "CommandBuffer.h"
#include "Buffer.h"
#include "Image.h"
#include "Handle.h"
#include "FrameManager.h"
#include <VMA/vk_mem_alloc.h>
#include <variant>
#include <unordered_map>
#include "GraphicsTypes.h"
#include "StagingBufferManager.h"
#include "TransferManager.h"

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
        const void* initialData,
        const std::vector<AssetLib::MipLevel>& mipLevels
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

    void reclaimStagingBuffers() {
        if (m_stagingManager) {
            m_stagingManager->reclaimBuffers();
        }
    }

    TransferManager& transferManager() {
        if (!m_transferManager) {
            throw std::runtime_error("TransferManager is not available");
        }
        return *m_transferManager;
    }

    VramHandle registerExternalImage(
        VkImage image,
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    std::unique_ptr<StagingBufferManager> m_stagingManager;
    std::unique_ptr<TransferManager> m_transferManager;
    
    VulkanContext& m_context;
    FrameManager& m_frameManager;
    CommandBufferManager& m_cmdBufferManager;
    SynchronizationResourceManager& m_syncResourceManager;
    std::unordered_map<uint64_t, Resource> m_resources;
    
    uint64_t m_nextId = 1;
};