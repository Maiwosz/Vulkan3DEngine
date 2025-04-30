#include "Image.h"
#include <stdexcept>
#include <cstring>
#include "CommandBuffer.h"

Image::Image() = default;

Image::Image(Image&& other) noexcept :
    AllocatedResource(std::move(other)),
    m_image(std::exchange(other.m_image, VK_NULL_HANDLE)),
    m_format(std::exchange(other.m_format, VK_FORMAT_UNDEFINED)),
    m_extent(std::exchange(other.m_extent, { 0, 0 })),
    m_samples(std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT)),
    m_currentLayout(std::exchange(other.m_currentLayout, VK_IMAGE_LAYOUT_UNDEFINED)),
    m_isExternal(std::exchange(other.m_isExternal, false))
{
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        AllocatedResource::operator=(std::move(other));
        m_image = other.m_image;
        m_format = other.m_format;
        m_extent = other.m_extent;
        m_currentLayout = other.m_currentLayout;
        m_samples = other.m_samples;
        m_isExternal = other.m_isExternal;

        // Reset the source object's state
        other.m_image = VK_NULL_HANDLE;
        other.m_format = VK_FORMAT_UNDEFINED;
        other.m_extent = { 0, 0 };
        other.m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.m_samples = VK_SAMPLE_COUNT_1_BIT;
        other.m_isExternal = false;
    }
    return *this;
}

VkAccessFlags Image::inferAccessMask(VkImageLayout layout) {
    switch (layout) {
        // Brak wymaganych operacji dostępu
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0;

        // Zapisy przez hosta (np. przy ładowaniu tekstur z CPU)
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return VK_ACCESS_HOST_WRITE_BIT;

        // Ogólny przypadek (np. obrazy storage)
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        // Attachment koloru
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Attachment głębi i stenila
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // Tylko do odczytu w shaderach
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;

        // Źródło operacji transferu
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;

        // Cel operacji transferu
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

        // Przygotowanie do prezentacji
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_ACCESS_MEMORY_READ_BIT;

        // Specjalne przypadki dla split depth/stencil
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // Rozszerzenia (np. dla Variable Rate Shading)
    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;

    default:
        throw std::runtime_error("Nieobsługiwany layout obrazu");
    }
}

void Image::destroyResourceImpl() {
    // Only destroy if we have a valid image AND a valid allocation
    // External images will have m_image but no m_allocation
    if (m_image != VK_NULL_HANDLE && m_allocation != nullptr) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }

    // Always reset the image handle
    m_image = VK_NULL_HANDLE;
}

Image Image::create(
    VmaAllocator allocator,
    const VkImageCreateInfo& imageInfo,
    VmaMemoryUsage memoryUsage,
    VkMemoryPropertyFlags requiredFlags)
{
    Image image;
    image.m_allocator = allocator;
    image.m_format = imageInfo.format;
    image.m_extent = { imageInfo.extent.width, imageInfo.extent.height };
    image.m_samples = imageInfo.samples;
    image.m_currentLayout = imageInfo.initialLayout;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = memoryUsage;
    allocCreateInfo.requiredFlags = requiredFlags;

    VkResult result = vmaCreateImage(
        allocator,
        &imageInfo,
        &allocCreateInfo,
        &image.m_image,
        &image.m_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image with VMA");
    }

    // Pobierz rozmiar alokacji
    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(allocator, image.m_allocation, &allocInfo);
    image.m_allocatedSize = allocInfo.size;

    return image;
}

Image Image::createExternal(
    VmaAllocator allocator,
    VkImage image,
    VkFormat format,
    VkExtent2D extent,
    VkImageLayout initialLayout,
    VkSampleCountFlagBits samples)
{
    Image externalImage;
    externalImage.m_allocator = allocator;
    externalImage.m_image = image;
    externalImage.m_format = format;
    externalImage.m_extent = extent;
    externalImage.m_currentLayout = initialLayout;
    externalImage.m_samples = samples;
    externalImage.m_allocation = nullptr;
    externalImage.m_isExternal = true;

    return externalImage;
}


VkImageView Image::createView(
    VkDevice device,
    VkImageViewType viewType,
    VkFormat format,
    VkImageAspectFlags aspectMask,
    uint32_t baseMipLevel,
    uint32_t mipLevels,
    uint32_t baseArrayLayer,
    uint32_t arrayLayers) const
{
    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = m_image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange = {
        .aspectMask = aspectMask,
        .baseMipLevel = baseMipLevel,
        .levelCount = mipLevels,
        .baseArrayLayer = baseArrayLayer,
        .layerCount = arrayLayers
    };

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
    return imageView;
}

void Image::recordLayoutTransition(
    CommandBuffer& commandBuffer,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkImageAspectFlags aspectMask
) {
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.image = m_image;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = newLayout;
    barrier.subresourceRange = { aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
    barrier.srcAccessMask = inferAccessMask(m_currentLayout);
    barrier.dstAccessMask = inferAccessMask(newLayout);

    vkCmdPipelineBarrier(
        commandBuffer.get(),
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    m_currentLayout = newLayout;
}


VkImageAspectFlags Image::getImageAspect(VkFormat format)
{
    if (format == VK_FORMAT_D32_SFLOAT) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}
