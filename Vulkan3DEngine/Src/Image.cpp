#include "Image.h"
#include <stdexcept>
#include <string>
#include "CommandBuffer.h"

Image::Image(Image&& other) noexcept
    : AllocatedResource(std::move(other))
    , m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
    , m_format(std::exchange(other.m_format, VK_FORMAT_UNDEFINED))
    , m_extent(std::exchange(other.m_extent, VkExtent2D{ 0, 0 }))
    , m_mipLevels(std::exchange(other.m_mipLevels, 1))
    , m_arrayLayers(std::exchange(other.m_arrayLayers, 1))
    , m_samples(std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT))
    , m_usage(std::exchange(other.m_usage, 0))
    , m_currentLayout(std::exchange(other.m_currentLayout, VK_IMAGE_LAYOUT_UNDEFINED))
    , m_isExternal(std::exchange(other.m_isExternal, false)) {
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        AllocatedResource::operator=(std::move(other));
        m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_format = std::exchange(other.m_format, VK_FORMAT_UNDEFINED);
        m_extent = std::exchange(other.m_extent, VkExtent2D{ 0, 0 });
        m_mipLevels = std::exchange(other.m_mipLevels, 1);
        m_arrayLayers = std::exchange(other.m_arrayLayers, 1);
        m_samples = std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT);
        m_usage = std::exchange(other.m_usage, 0);
        m_currentLayout = std::exchange(other.m_currentLayout, VK_IMAGE_LAYOUT_UNDEFINED);
        m_isExternal = std::exchange(other.m_isExternal, false);
    }
    return *this;
}

Image Image::create(VmaAllocator allocator, const VkImageCreateInfo& imageInfo,
    VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags requiredFlags) {
    if (imageInfo.extent.width == 0 || imageInfo.extent.height == 0) {
        throw std::runtime_error("Invalid image dimensions");
    }

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.requiredFlags = requiredFlags;

    Image image;
    VkResult result = vmaCreateImage(allocator, &imageInfo, &allocInfo,
        &image.m_image, &image.m_allocation, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image: " + std::to_string(result));
    }

    image.initializeAllocation(allocator, image.m_allocation);
    image.m_format = imageInfo.format;
    image.m_extent = { imageInfo.extent.width, imageInfo.extent.height };
    image.m_mipLevels = imageInfo.mipLevels;
    image.m_arrayLayers = imageInfo.arrayLayers;
    image.m_samples = imageInfo.samples;
    image.m_usage = imageInfo.usage;
    image.m_currentLayout = imageInfo.initialLayout;
    image.m_isExternal = false;

    return image;
}

Image Image::createExternal(VmaAllocator allocator, VkImage image, VkFormat format,
    VkExtent2D extent, VkImageLayout initialLayout,
    VkSampleCountFlagBits samples,
    uint32_t mipLevels, uint32_t arrayLayers) {
    Image externalImage;
    externalImage.m_allocator = allocator;
    externalImage.m_image = image;
    externalImage.m_format = format;
    externalImage.m_extent = extent;
    externalImage.m_mipLevels = mipLevels;
    externalImage.m_arrayLayers = arrayLayers;
    externalImage.m_samples = samples;
    externalImage.m_currentLayout = initialLayout;
    externalImage.m_isExternal = true;

    return externalImage;
}

VkImageView Image::createView(VkDevice device, VkImageViewType viewType,
    VkFormat format, VkImageAspectFlags aspectMask,
    uint32_t baseMipLevel, uint32_t levelCount,
    uint32_t baseArrayLayer, uint32_t layerCount) const {

    VkFormat viewFormat = (format == VK_FORMAT_UNDEFINED) ? m_format : format;
    VkImageAspectFlags viewAspectMask = aspectMask ? aspectMask : getImageAspect(viewFormat);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = viewType;
    viewInfo.format = viewFormat;
    viewInfo.subresourceRange = {
        .aspectMask = viewAspectMask,
        .baseMipLevel = baseMipLevel,
        .levelCount = levelCount,
        .baseArrayLayer = baseArrayLayer,
        .layerCount = layerCount
    };

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

void Image::recordLayoutTransition(CommandBuffer& cmdBuffer, VkImageLayout newLayout,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    VkImageAspectFlags aspectMask) {
    if (m_currentLayout == newLayout) return;

    VkImageAspectFlags imageAspect = aspectMask ? aspectMask : getImageAspect(m_format);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange = {
        .aspectMask = imageAspect,
        .baseMipLevel = 0,
        .levelCount = m_mipLevels,
        .baseArrayLayer = 0,
        .layerCount = m_arrayLayers
    };
    barrier.srcAccessMask = getAccessMask(m_currentLayout);
    barrier.dstAccessMask = getAccessMask(newLayout);

    // You'll need to implement cmdBuffer.get() to return VkCommandBuffer
    vkCmdPipelineBarrier(cmdBuffer.handle(), srcStage, dstStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    m_currentLayout = newLayout;
}

VkImageAspectFlags Image::getImageAspect(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkAccessFlags Image::getAccessMask(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0;
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_ACCESS_MEMORY_READ_BIT;
    default:
        return 0;
    }
}

void Image::destroyResource() {
    if (m_image != VK_NULL_HANDLE && m_allocator && !m_isExternal) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
    resetImageState();
}

void Image::resetImageState() {
    m_image = VK_NULL_HANDLE;
    m_format = VK_FORMAT_UNDEFINED;
    m_extent = { 0, 0 };
    m_mipLevels = 1;
    m_arrayLayers = 1;
    m_samples = VK_SAMPLE_COUNT_1_BIT;
    m_usage = 0;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_isExternal = false;
}