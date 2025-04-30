#pragma once
#include "AllocatedResource.h"
#include <vulkan/vulkan.h>

class CommandBuffer;

class Image : public AllocatedResource {
public:
    Image();
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image() = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    static Image create(
        VmaAllocator allocator,
        const VkImageCreateInfo& imageInfo,
        VmaMemoryUsage memoryUsage,
        VkMemoryPropertyFlags requiredFlags = 0);

    static Image createExternal(
        VmaAllocator allocator,
        VkImage image,
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

    VkImage get() const { return m_image; }
    VkFormat getFormat() const { return m_format; }
    VkExtent2D getExtent() const { return m_extent; }
    VkImageLayout getCurrentLayout() const { return m_currentLayout; }
    VkSampleCountFlagBits getSamples() const { return m_samples; }
    bool isExternalResource() const { return m_isExternal; }

    VkImageView createView(
        VkDevice device,
        VkImageViewType viewType,
        VkFormat format,
        VkImageAspectFlags aspectMask,
        uint32_t baseMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
        uint32_t baseArrayLayer = 0,
        uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS) const;

    void recordLayoutTransition(
        CommandBuffer& commandBuffer,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    );

    // Helper functions
    static VkImageAspectFlags getImageAspect(VkFormat format);
    static VkAccessFlags inferAccessMask(VkImageLayout layout);

protected:
    void destroyResourceImpl() override;

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = { 0, 0 };
    VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;

    bool m_isExternal = false;
};