#pragma once
#include "AllocatedResource.h"
#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <utility>

// Forward declarations
class CommandBuffer;

// Image class
class Image : public AllocatedResource {
public:
    Image() = default;
    ~Image() { cleanup(); }

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    // Factory methods
    static Image create(VmaAllocator allocator, const VkImageCreateInfo& imageInfo,
        VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags requiredFlags = 0);

    static Image createExternal(VmaAllocator allocator, VkImage image, VkFormat format,
        VkExtent2D extent, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

    // Image view creation
    VkImageView createView(VkDevice device, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
        VkFormat format = VK_FORMAT_UNDEFINED,
        VkImageAspectFlags aspectMask = 0,
        uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
        uint32_t baseArrayLayer = 0, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS) const;

    // Layout transition recording
    void recordLayoutTransition(CommandBuffer& cmdBuffer, VkImageLayout newLayout,
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VkImageAspectFlags aspectMask = 0);

    // Accessors
    VkImage get() const noexcept { return m_image; }
    VkFormat getFormat() const noexcept { return m_format; }
    VkExtent2D getExtent() const noexcept { return m_extent; }
    uint32_t getMipLevels() const noexcept { return m_mipLevels; }
    uint32_t getArrayLayers() const noexcept { return m_arrayLayers; }
    VkSampleCountFlagBits getSamples() const noexcept { return m_samples; }
    VkImageUsageFlags getUsage() const noexcept { return m_usage; }
    VkImageLayout getCurrentLayout() const noexcept { return m_currentLayout; }
    bool isExternalResource() const noexcept { return m_isExternal; }

    void setLayout(VkImageLayout layout) noexcept { m_currentLayout = layout; }
    explicit operator bool() const noexcept { return m_image != VK_NULL_HANDLE; }

    // Static utility functions
    static VkImageAspectFlags getImageAspect(VkFormat format);
    static VkAccessFlags getAccessMask(VkImageLayout layout);

protected:
    void destroyResource() override;

private:
    void resetImageState();

    VkImage m_image = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = { 0, 0 };
    uint32_t m_mipLevels = 1;
    uint32_t m_arrayLayers = 1;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags m_usage = 0;
    VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool m_isExternal = false;
};