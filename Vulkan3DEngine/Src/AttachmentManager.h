#pragma once
#include "VramManager.h"
#include "Image.h"
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
    std::string name;  // Optional name for debugging

    bool operator==(const AttachmentSpec& other) const {
        return format == other.format &&
            extent.width == other.extent.width &&
            extent.height == other.extent.height &&
            samples == other.samples &&
            usage == other.usage &&
            initialLayout == other.initialLayout &&
            finalLayout == other.finalLayout &&
            type == other.type;
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
            return hash;
        }

    private:
        // Helper for combining hash values
        template <typename T>
        void hash_combine(size_t& seed, const T& val) const {
            seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}

struct AttachmentHandle {
    uint32_t id;

    constexpr explicit AttachmentHandle(uint32_t id = 0) : id(id) {}

    bool operator==(const AttachmentHandle&) const = default;
    bool operator<(const AttachmentHandle& other) const { return id < other.id; }
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<AttachmentHandle> {
        size_t operator()(const AttachmentHandle& handle) const {
            return hash<uint32_t>()(handle.id);
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

class AttachmentManager {
public:
    AttachmentManager(const LogicalDevice& device, VramManager& vramManager);
    ~AttachmentManager();

    // Delete copy constructors
    AttachmentManager(const AttachmentManager&) = delete;
    AttachmentManager& operator=(const AttachmentManager&) = delete;

    // Create or get an attachment based on specification
    AttachmentHandle getOrCreate(const AttachmentSpec& spec);

    // Register an external image as an attachment (for swapchain images)
    AttachmentHandle registerExternalImage(
        VramHandle imageHandle,
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        AttachmentType type = AttachmentType::Color,
        const std::string& name = "SwapchainAttachment");

    // Get attachment by handle
    const Attachment* get(AttachmentHandle handle) const;

    // Check if handle is valid
    bool isValid(AttachmentHandle handle) const;

    // Destroy an attachment
    void destroy(AttachmentHandle handle);

    // Clean up all attachments
    void cleanup();

    // Get all attachments
    const std::unordered_map<AttachmentHandle, std::unique_ptr<Attachment>>& getAllAttachments() const {
        return m_attachments;
    }

    // Handle window resize events - recreate attachments as needed
    void onResize(VkExtent2D newExtent);

private:
    // Create a new attachment from a specification
    AttachmentHandle createAttachment(const AttachmentSpec& spec);

    // Helper functions
    VkImageUsageFlags getDefaultUsageFlags(AttachmentType type) const;
    VkImageView createImageView(VramHandle imageHandle, const AttachmentSpec& spec);

    const LogicalDevice& m_device;
    VramManager& m_vramManager;

    // Maps from spec to handle for quick lookup
    std::unordered_map<AttachmentSpec, AttachmentHandle> m_specToHandle;

    // Maps from handle to attachment objects
    std::unordered_map<AttachmentHandle, std::unique_ptr<Attachment>> m_attachments;

    // Maps for keeping track of size-dependent attachments
    std::vector<AttachmentHandle> m_sizeDependent;

    uint32_t m_nextHandleId = 1;
};