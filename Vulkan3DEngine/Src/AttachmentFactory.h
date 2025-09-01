#pragma once
#include <vulkan/vulkan.h>

// Forward declarations
enum class AttachmentType;
struct AttachmentSpec;

class AttachmentFactory {
public:
    AttachmentFactory() = default;
    ~AttachmentFactory() = default;

    // Delete copy constructors
    AttachmentFactory(const AttachmentFactory&) = delete;
    AttachmentFactory& operator=(const AttachmentFactory&) = delete;

    // Color attachment creation methods
    AttachmentSpec createColorAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0,  // 0 = use default
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE
    );

    // Depth attachment creation methods
    AttachmentSpec createDepthAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0,  // 0 = use default
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    );

    // Depth-stencil attachment creation methods
    AttachmentSpec createDepthStencilAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0,  // 0 = use default
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    );

    // Resolve attachment creation methods
    AttachmentSpec createResolveAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkImageUsageFlags usage = 0,  // 0 = use default
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE
    );

    // Swapchain color attachment (for external images)
    AttachmentSpec createSwapchainColorAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE
    );

    // Convenience methods for common use cases
    AttachmentSpec createMSAAColorAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples
    );

    AttachmentSpec createMSAADepthAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples
    );

    // Input attachment (for reading in shaders)
    AttachmentSpec createInputAttachment(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Helper method to get default usage flags for attachment type
    VkImageUsageFlags getDefaultUsageFlags(AttachmentType type) const;
};