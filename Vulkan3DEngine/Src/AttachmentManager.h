#pragma once
#include "VramManager.h"
#include "Image.h"
#include "IResourceManager.h"
#include "Handle.h"
#include "AttachmentFactory.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

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

struct AttachmentSpec {
    VkFormat format;
    VkExtent2D extent;
    VkSampleCountFlagBits samples;
    VkImageUsageFlags usage;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
    AttachmentType type;

    // Render pass specific operations
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    bool operator==(const AttachmentSpec& other) const {
        return format == other.format &&
            extent.width == other.extent.width &&
            extent.height == other.extent.height &&
            samples == other.samples &&
            usage == other.usage &&
            initialLayout == other.initialLayout &&
            finalLayout == other.finalLayout &&
            type == other.type &&
            loadOp == other.loadOp &&
            storeOp == other.storeOp &&
            stencilLoadOp == other.stencilLoadOp &&
            stencilStoreOp == other.stencilStoreOp;
    }
};

// Custom hash implementation for AttachmentSpec
namespace std {
    template<> struct hash<AttachmentSpec> {
        size_t operator()(const AttachmentSpec& spec) const {
            size_t hash = 0;
            hash_combine(hash, spec.format);
            hash_combine(hash, spec.extent.width);
            hash_combine(hash, spec.extent.height);
            hash_combine(hash, spec.samples);
            hash_combine(hash, spec.usage);
            hash_combine(hash, static_cast<int>(spec.initialLayout));
            hash_combine(hash, static_cast<int>(spec.finalLayout));
            hash_combine(hash, static_cast<int>(spec.type));
            hash_combine(hash, static_cast<int>(spec.loadOp));
            hash_combine(hash, static_cast<int>(spec.storeOp));
            hash_combine(hash, static_cast<int>(spec.stencilLoadOp));
            hash_combine(hash, static_cast<int>(spec.stencilStoreOp));
            return hash;
        }

    private:
        template <typename T>
        void hash_combine(size_t& seed, const T& val) const {
            seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}

class Attachment {
public:
    Attachment(VramHandle imageHandle, VkImageView imageView, const AttachmentSpec& spec)
        : m_imageHandle(imageHandle), m_imageView(imageView), m_spec(spec) {
    }

    ~Attachment() = default;

    VramHandle getImageHandle() const { return m_imageHandle; }
    VkImageView getImageView() const { return m_imageView; }
    const AttachmentSpec& getSpec() const { return m_spec; }

    // Get image aspect based on attachment type
    VkImageAspectFlags getAspectMask() const;

private:
    VramHandle m_imageHandle;
    VkImageView m_imageView;
    AttachmentSpec m_spec;
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
    // Create or get an attachment based on specification
    AttachmentHandle acquireAttachment(const AttachmentSpec& spec);

    // Register an external image as an attachment (for swapchain images)
    AttachmentHandle registerExternalImage(
        VramHandle imageHandle,
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        AttachmentType type = AttachmentType::Color
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
    AttachmentHandle createAttachment(const AttachmentSpec& spec);

    // Helper functions
    VkImageView createImageView(VramHandle imageHandle, const AttachmentSpec& spec);
    void destroyAttachment(AttachmentHandle handle);

    const LogicalDevice& m_device;
    VramManager& m_vramManager;
    AttachmentFactory m_factory;  // Attachment factory instance

    // Maps from spec to handle for quick lookup
    std::unordered_map<AttachmentSpec, AttachmentHandle> m_specToHandle;

    // Maps from handle to attachment objects with reference counting
    std::unordered_map<AttachmentHandle, AttachmentData> m_attachments;

    // Maps for keeping track of size-dependent attachments
    std::vector<AttachmentHandle> m_sizeDependent;

    uint32_t m_nextHandleId = 1;
};