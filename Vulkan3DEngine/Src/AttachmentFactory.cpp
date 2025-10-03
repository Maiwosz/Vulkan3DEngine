#include "AttachmentFactory.h"
#include "AttachmentManager.h"

// Image specification creation methods
AttachmentImageSpec AttachmentFactory::createColorImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = samples;
    spec.usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Color) : usage;
    spec.type = AttachmentType::Color;
    return spec;
}

AttachmentImageSpec AttachmentFactory::createDepthImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = samples;
    spec.usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Depth) : usage;
    spec.type = AttachmentType::Depth;
    return spec;
}

AttachmentImageSpec AttachmentFactory::createDepthStencilImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = samples;
    spec.usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::DepthStencil) : usage;
    spec.type = AttachmentType::DepthStencil;
    return spec;
}

AttachmentImageSpec AttachmentFactory::createResolveImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usage
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = VK_SAMPLE_COUNT_1_BIT; // Resolve attachments are always single-sampled
    spec.usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Resolve) : usage;
    spec.type = AttachmentType::Resolve;
    return spec;
}

AttachmentImageSpec AttachmentFactory::createSwapchainImageSpec(
    VkFormat format,
    VkExtent2D extent
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = VK_SAMPLE_COUNT_1_BIT;
    spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Swapchain images are color attachments
    spec.type = AttachmentType::Color;
    return spec;
}

// Render pass attachment creation methods
RenderPassAttachment AttachmentFactory::createColorAttachment(
    const AttachmentImageSpec& imageSpec,
    uint32_t imageIndex,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentDescriptionFlags flags
) {
    return RenderPassAttachment(
        imageSpec,
        initialLayout,
        finalLayout,
        loadOp,
        storeOp,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilStoreOp
        flags,
        imageIndex
    );
}

RenderPassAttachment AttachmentFactory::createDepthAttachment(
    const AttachmentImageSpec& imageSpec,
    uint32_t imageIndex,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentDescriptionFlags flags
) {
    return RenderPassAttachment(
        imageSpec,
        initialLayout,
        finalLayout,
        loadOp,
        storeOp,
        loadOp,    // stencilLoadOp - use same as depth for depth-only
        storeOp,   // stencilStoreOp - use same as depth for depth-only
        flags,
        imageIndex
    );
}

RenderPassAttachment AttachmentFactory::createDepthStencilAttachment(
    const AttachmentImageSpec& imageSpec,
    uint32_t imageIndex,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp depthLoadOp,
    VkAttachmentStoreOp depthStoreOp,
    VkAttachmentLoadOp stencilLoadOp,
    VkAttachmentStoreOp stencilStoreOp,
    VkAttachmentDescriptionFlags flags
) {
    return RenderPassAttachment(
        imageSpec,
        initialLayout,
        finalLayout,
        depthLoadOp,
        depthStoreOp,
        stencilLoadOp,
        stencilStoreOp,
        flags,
        imageIndex
    );
}

RenderPassAttachment AttachmentFactory::createResolveAttachment(
    const AttachmentImageSpec& imageSpec,
    uint32_t imageIndex,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentDescriptionFlags flags
) {
    return RenderPassAttachment(
        imageSpec,
        initialLayout,
        finalLayout,
        loadOp,
        storeOp,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilStoreOp
        flags,
        imageIndex
    );
}

RenderPassAttachment AttachmentFactory::createSwapchainAttachment(
    const AttachmentImageSpec& imageSpec,
    uint32_t imageIndex,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentDescriptionFlags flags
) {
    return RenderPassAttachment(
        imageSpec,
        initialLayout,
        finalLayout,
        loadOp,
        storeOp,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilStoreOp
        flags,
        imageIndex
    );
}

// Convenience methods for common use cases
AttachmentImageSpec AttachmentFactory::createMSAAColorImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples
) {
    return createColorImageSpec(
        format,
        extent,
        samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
    );
}

AttachmentImageSpec AttachmentFactory::createMSAADepthImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples
) {
    return createDepthImageSpec(
        format,
        extent,
        samples,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
    );
}

AttachmentImageSpec AttachmentFactory::createInputImageSpec(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples
) {
    AttachmentImageSpec spec{};
    spec.format = format;
    spec.extent = extent;
    spec.samples = samples;
    spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    spec.type = AttachmentType::Color;
    return spec;
}

VkImageUsageFlags AttachmentFactory::getDefaultUsageFlags(AttachmentType type) const {
    switch (type) {
    case AttachmentType::Depth:
    case AttachmentType::DepthStencil:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    case AttachmentType::Resolve:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case AttachmentType::Color:
    default:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
}