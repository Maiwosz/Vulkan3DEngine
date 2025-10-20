#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "VramManager.h"
#include "Image.h"
#include "IResourceManager.h"
#include "Handle.h"
#include "AttachmentFactory.h"

// Forward declarations
class VulkanContext;
class LogicalDevice;

// Attachment types and specifications
enum class AttachmentType {
    Color,
    Depth,
    DepthStencil,
    Resolve
};

// Structure for image creation (separated from render pass concerns)
struct AttachmentImageSpec {
    VkFormat format;
    VkExtent2D extent;
    VkSampleCountFlagBits samples;
    VkImageUsageFlags usage;
    AttachmentType type;

    bool operator==(const AttachmentImageSpec& other) const {
        return format == other.format &&
            extent.width == other.extent.width &&
            extent.height == other.extent.height &&
            samples == other.samples &&
            usage == other.usage &&
            type == other.type;
    }

    size_t hash() const {
        size_t hash = 0;
        hash_combine(hash, format);
        hash_combine(hash, extent.width);
        hash_combine(hash, extent.height);
        hash_combine(hash, samples);
        hash_combine(hash, usage);
        hash_combine(hash, static_cast<int>(type));
        return hash;
    }

private:
    template <typename T>
    void hash_combine(size_t& seed, const T& val) const {
        seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

// Structure for render pass attachment (extends VkAttachmentDescription)
struct RenderPassAttachment {
    VkAttachmentDescription desc;
    uint32_t imageIndex;  // Index into the image array/vector

    RenderPassAttachment() = default;

    RenderPassAttachment(const AttachmentImageSpec& imageSpec,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VkAttachmentDescriptionFlags flags = 0,
        uint32_t imgIndex = 0)
        : imageIndex(imgIndex) {
        desc.flags = flags;
        desc.format = imageSpec.format;
        desc.samples = imageSpec.samples;
        desc.loadOp = loadOp;
        desc.storeOp = storeOp;
        desc.stencilLoadOp = stencilLoadOp;
        desc.stencilStoreOp = stencilStoreOp;
        desc.initialLayout = initialLayout;
        desc.finalLayout = finalLayout;
    }

    bool operator==(const RenderPassAttachment& other) const {
        return desc.flags == other.desc.flags &&
            desc.format == other.desc.format &&
            desc.samples == other.desc.samples &&
            desc.loadOp == other.desc.loadOp &&
            desc.storeOp == other.desc.storeOp &&
            desc.stencilLoadOp == other.desc.stencilLoadOp &&
            desc.stencilStoreOp == other.desc.stencilStoreOp &&
            desc.initialLayout == other.desc.initialLayout &&
            desc.finalLayout == other.desc.finalLayout &&
            imageIndex == other.imageIndex;
    }

    size_t hash() const {
        size_t hash = 0;
        hash_combine(hash, desc.flags);
        hash_combine(hash, desc.format);
        hash_combine(hash, desc.samples);
        hash_combine(hash, static_cast<int>(desc.loadOp));
        hash_combine(hash, static_cast<int>(desc.storeOp));
        hash_combine(hash, static_cast<int>(desc.stencilLoadOp));
        hash_combine(hash, static_cast<int>(desc.stencilStoreOp));
        hash_combine(hash, static_cast<int>(desc.initialLayout));
        hash_combine(hash, static_cast<int>(desc.finalLayout));
        hash_combine(hash, imageIndex);
        return hash;
    }

private:
    template <typename T>
    void hash_combine(size_t& seed, const T& val) const {
        seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

// Custom hash implementations
namespace std {
    template<> struct hash<AttachmentImageSpec> {
        size_t operator()(const AttachmentImageSpec& spec) const {
            return spec.hash();
        }
    };

    template<> struct hash<RenderPassAttachment> {
        size_t operator()(const RenderPassAttachment& attachment) const {
            return attachment.hash();
        }
    };
}

class Attachment {
public:
    Attachment(VramHandle imageHandle, VkImageView imageView, const AttachmentImageSpec& spec)
        : m_imageHandle(imageHandle), m_imageView(imageView), m_spec(spec) {
    }

    ~Attachment() = default;

    VramHandle getImageHandle() const { return m_imageHandle; }
    VkImageView getImageView() const { return m_imageView; }
    const AttachmentImageSpec& getSpec() const { return m_spec; }

    // Get image aspect based on attachment type
    VkImageAspectFlags getAspectMask() const;

private:
    VramHandle m_imageHandle;
    VkImageView m_imageView;
    AttachmentImageSpec m_spec;
};

class AttachmentManager : public IResourceManager<AttachmentHandle, Attachment> {
public:
    AttachmentManager(const LogicalDevice& device, VramManager& vramManager);
    ~AttachmentManager();

    // Delete copy constructors
    AttachmentManager(const AttachmentManager&) = delete;
    AttachmentManager& operator=(const AttachmentManager&) = delete;

    // IResourceManager interface implementation
    Attachment* getResource(AttachmentHandle handle) override;
    bool isValid(AttachmentHandle handle) const override;
    void releaseResource(AttachmentHandle handle) override;
    void addReference(AttachmentHandle handle) override;
    void removeReference(AttachmentHandle handle) override;

    // AttachmentManager specific methods
    // Create or get an attachment based on image specification
    AttachmentHandle acquireAttachment(const AttachmentImageSpec& spec);

    // Register an external image as an attachment (for swapchain images)
    AttachmentHandle registerExternalImage(
        VramHandle imageHandle,
        VkFormat format,
        VkExtent2D extent,
        AttachmentType type = AttachmentType::Color,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );

    void updateExternalImage(
        AttachmentHandle handle,
        VramHandle newImageHandle
    );

    // Access to the attachment factory
    AttachmentFactory& getFactory() { return m_factory; }
    const AttachmentFactory& getFactory() const { return m_factory; }

private:
    struct AttachmentData {
        std::unique_ptr<Attachment> attachment;
        uint32_t referenceCount;

        AttachmentData(std::unique_ptr<Attachment> att)
            : attachment(std::move(att)), referenceCount(1) {
        }
    };

    // Create a new attachment from a specification
    AttachmentHandle createAttachment(const AttachmentImageSpec& spec);

    // Helper functions
    VkImageView createImageView(VramHandle imageHandle, const AttachmentImageSpec& spec);
    void destroyAttachment(AttachmentHandle handle);

    const LogicalDevice& m_device;
    VramManager& m_vramManager;
    AttachmentFactory m_factory;  // Attachment factory instance

    // Maps from spec to handle for quick lookup
    std::unordered_map<AttachmentImageSpec, AttachmentHandle> m_specToHandle;

    // Maps from handle to attachment objects with reference counting
    std::unordered_map<AttachmentHandle, AttachmentData> m_attachments;

    // Maps for keeping track of size-dependent attachments
    std::vector<AttachmentHandle> m_sizeDependent;

    uint32_t m_nextHandleId = 1;
};