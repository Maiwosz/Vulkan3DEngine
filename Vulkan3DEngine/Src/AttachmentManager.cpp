#include "AttachmentManager.h"
#include "LogicalDevice.h"
#include <algorithm>
#include <cassert>

VkImageAspectFlags Attachment::getAspectMask() const {
    switch (m_spec.type) {
    case AttachmentType::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case AttachmentType::DepthStencil:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case AttachmentType::Color:
    case AttachmentType::Resolve:
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

AttachmentManager::AttachmentManager(const LogicalDevice& device, VramManager& vramManager)
    : m_device(device), m_vramManager(vramManager) {
}

AttachmentManager::~AttachmentManager() {
    // Clear size-dependent list
    m_sizeDependent.clear();

    // Clear specs map
    m_specToHandle.clear();

    // Destroy all attachments
    for (auto& [handle, attachmentData] : m_attachments) {
        // Destroy the image view
        vkDestroyImageView(m_device.get(), attachmentData.attachment->getImageView(), nullptr);

        // Free the image resource
        m_vramManager.freeResource(attachmentData.attachment->getImageHandle());
    }

    // Clear the map
    m_attachments.clear();
}

// IResourceManager interface implementation
Attachment* AttachmentManager::getResource(AttachmentHandle handle) {
    auto it = m_attachments.find(handle);
    if (it != m_attachments.end()) {
        return it->second.attachment.get();
    }
    return nullptr;
}

bool AttachmentManager::isValid(AttachmentHandle handle) const {
    return m_attachments.find(handle) != m_attachments.end();
}

void AttachmentManager::releaseResource(AttachmentHandle handle) {
    removeReference(handle);
}

void AttachmentManager::addReference(AttachmentHandle handle) {
    auto it = m_attachments.find(handle);
    if (it != m_attachments.end()) {
        ++it->second.referenceCount;
    }
}

void AttachmentManager::removeReference(AttachmentHandle handle) {
    auto it = m_attachments.find(handle);
    if (it != m_attachments.end()) {
        --it->second.referenceCount;
        if (it->second.referenceCount == 0) {
            destroyAttachment(handle);
        }
    }
}

// AttachmentManager specific methods
AttachmentHandle AttachmentManager::acquireAttachment(const AttachmentImageSpec& spec) {
    // Check if an attachment with this spec already exists
    auto it = m_specToHandle.find(spec);
    if (it != m_specToHandle.end()) {
        addReference(it->second);
        return it->second;
    }

    // Create a new attachment
    return createAttachment(spec);
}

AttachmentHandle AttachmentManager::registerExternalImage(
    VramHandle imageHandle,
    VkFormat format,
    VkExtent2D extent,
    AttachmentType type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage
) {
    // Create image spec for this external image
    AttachmentImageSpec spec;
    spec.format = format;
    spec.extent = extent;
    spec.samples = samples;
    spec.usage = usage;
    spec.type = type;

    // Create image view for this external image
    VkImageView imageView = createImageView(imageHandle, spec);

    // Create attachment handle
    AttachmentHandle handle(m_nextHandleId++);

    // Create attachment object
    auto attachment = std::make_unique<Attachment>(imageHandle, imageView, spec);

    // Store attachment with reference counting
    m_attachments.emplace(handle, AttachmentData(std::move(attachment)));
    m_specToHandle[spec] = handle;

    return handle;
}

void AttachmentManager::recreateAttachment(
    AttachmentHandle handle,
    const AttachmentImageSpec& newSpec
) {
    auto it = m_attachments.find(handle);
    if (it == m_attachments.end()) {
        return; // Handle doesn't exist
    }

    AttachmentData& data = it->second;
    const AttachmentImageSpec& oldSpec = data.attachment->getSpec();

    // Remove old spec mapping
    m_specToHandle.erase(oldSpec);

    // Destroy old image view
    vkDestroyImageView(m_device.get(), data.attachment->getImageView(), nullptr);

    // Free old image resource
    m_vramManager.freeResource(data.attachment->getImageHandle());

    // Determine image usage flags
    VkImageUsageFlags usage = newSpec.usage;
    if (usage == 0) {
        usage = m_factory.getDefaultUsageFlags(newSpec.type);
    }

    // Create new image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = newSpec.extent.width;
    imageInfo.extent.height = newSpec.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = newSpec.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = newSpec.samples;
    imageInfo.flags = 0;

    VramHandle newImageHandle = m_vramManager.createImage(
        imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create new image view
    VkImageView newImageView = createImageView(newImageHandle, newSpec);

    // Update attachment with new data
    data.attachment = std::make_unique<Attachment>(newImageHandle, newImageView, newSpec);

    // Add new spec mapping
    m_specToHandle[newSpec] = handle;
}

AttachmentHandle AttachmentManager::createAttachment(const AttachmentImageSpec& spec) {
    // Ensure the attachment doesn't already exist
    assert(m_specToHandle.find(spec) == m_specToHandle.end());

    // Determine image usage flags
    VkImageUsageFlags usage = spec.usage;
    if (usage == 0) {
        usage = m_factory.getDefaultUsageFlags(spec.type);
    }

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = spec.extent.width;
    imageInfo.extent.height = spec.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = spec.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = spec.samples;
    imageInfo.flags = 0;

    // Create image in VRAM
    VramHandle imageHandle = m_vramManager.createImage(
        imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create image view
    VkImageView imageView = createImageView(imageHandle, spec);

    // Create attachment handle
    AttachmentHandle handle(m_nextHandleId++);

    // Create attachment object
    auto attachment = std::make_unique<Attachment>(imageHandle, imageView, spec);

    // Store attachment with reference counting
    m_attachments.emplace(handle, AttachmentData(std::move(attachment)));
    m_specToHandle[spec] = handle;

    // If this attachment depends on window size, track it for recreation
    if (spec.extent.width > 0 && spec.extent.height > 0) {
        m_sizeDependent.push_back(handle);
    }

    return handle;
}

void AttachmentManager::destroyAttachment(AttachmentHandle handle) {
    auto it = m_attachments.find(handle);
    if (it != m_attachments.end()) {
        // Remove from size-dependent list if present
        m_sizeDependent.erase(
            std::remove(m_sizeDependent.begin(), m_sizeDependent.end(), handle),
            m_sizeDependent.end());

        // Remove from specs map
        for (auto specIt = m_specToHandle.begin(); specIt != m_specToHandle.end(); ) {
            if (specIt->second == handle) {
                specIt = m_specToHandle.erase(specIt);
            }
            else {
                ++specIt;
            }
        }

        // Get attachment info
        const Attachment& attachment = *it->second.attachment;

        // Destroy the image view
        vkDestroyImageView(m_device.get(), attachment.getImageView(), nullptr);

        // Free the image resource
        m_vramManager.freeResource(attachment.getImageHandle());

        // Remove the attachment from the map
        m_attachments.erase(it);
    }
}

VkImageView AttachmentManager::createImageView(VramHandle imageHandle, const AttachmentImageSpec& spec) {
    // Get the image from VRAM manager
    Image* image = m_vramManager.getResource<Image>(imageHandle);
    if (!image) {
        return VK_NULL_HANDLE;
    }

    // Determine image aspect flags based on attachment type
    VkImageAspectFlags aspectFlags;
    switch (spec.type) {
    case AttachmentType::Depth:
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    case AttachmentType::DepthStencil:
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    case AttachmentType::Color:
    case AttachmentType::Resolve:
    default:
        aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    }

    // Create image view
    return image->createView(
        m_device.get(),
        VK_IMAGE_VIEW_TYPE_2D,
        spec.format,
        aspectFlags
    );
}