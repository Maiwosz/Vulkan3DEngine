#pragma once
#include <vulkan/vulkan.h>

// Forward declarations
enum class AttachmentType;
struct AttachmentImageSpec;
struct RenderPassAttachment;

class AttachmentFactory {
public:
    AttachmentFactory() = default;
    ~AttachmentFactory() = default;

    // Delete copy constructors
    AttachmentFactory(const AttachmentFactory&) = delete;
    AttachmentFactory& operator=(const AttachmentFactory&) = delete;

    // Image specification creation methods
    AttachmentImageSpec createColorImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0  // 0 = use default
    );

    AttachmentImageSpec createDepthImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0  // 0 = use default
    );

    AttachmentImageSpec createDepthStencilImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageUsageFlags usage = 0  // 0 = use default
    );

    AttachmentImageSpec createResolveImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkImageUsageFlags usage = 0  // 0 = use default
    );

    AttachmentImageSpec createSwapchainImageSpec(
        VkFormat format,
        VkExtent2D extent
    );

    // Render pass attachment creation methods (combines image spec with render pass parameters)
    RenderPassAttachment createColorAttachment(
        const AttachmentImageSpec& imageSpec,
        uint32_t imageIndex = 0,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        VkAttachmentDescriptionFlags flags = 0
    );

    RenderPassAttachment createDepthAttachment(
        const AttachmentImageSpec& imageSpec,
        uint32_t imageIndex = 0,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VkAttachmentDescriptionFlags flags = 0
    );

    RenderPassAttachment createDepthStencilAttachment(
        const AttachmentImageSpec& imageSpec,
        uint32_t imageIndex = 0,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VkAttachmentDescriptionFlags flags = 0
    );

    RenderPassAttachment createResolveAttachment(
        const AttachmentImageSpec& imageSpec,
        uint32_t imageIndex = 0,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        VkAttachmentDescriptionFlags flags = 0
    );

    RenderPassAttachment createSwapchainAttachment(
        const AttachmentImageSpec& imageSpec,
        uint32_t imageIndex = 0,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        VkAttachmentDescriptionFlags flags = 0
    );

    // Convenience methods for common use cases
    AttachmentImageSpec createMSAAColorImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples
    );

    AttachmentImageSpec createMSAADepthImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples
    );

    // Input attachment spec (for reading in shaders)
    AttachmentImageSpec createInputImageSpec(
        VkFormat format,
        VkExtent2D extent,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
    );

    // Helper method to get default usage flags for attachment type
    VkImageUsageFlags getDefaultUsageFlags(AttachmentType type) const;
};