#include "AttachmentFactory.h"
#include "AttachmentManager.h"  // For AttachmentSpec and AttachmentType

AttachmentSpec AttachmentFactory::createColorAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = samples,
        .usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Color) : usage,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::Color,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    };
}

AttachmentSpec AttachmentFactory::createDepthAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = samples,
        .usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Depth) : usage,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::Depth,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = loadOp,  // For depth-only, use same as depth
        .stencilStoreOp = storeOp
    };
}

AttachmentSpec AttachmentFactory::createDepthStencilAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp depthLoadOp,
    VkAttachmentStoreOp depthStoreOp,
    VkAttachmentLoadOp stencilLoadOp,
    VkAttachmentStoreOp stencilStoreOp
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = samples,
        .usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::DepthStencil) : usage,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::DepthStencil,
        .loadOp = depthLoadOp,
        .storeOp = depthStoreOp,
        .stencilLoadOp = stencilLoadOp,
        .stencilStoreOp = stencilStoreOp
    };
}

AttachmentSpec AttachmentFactory::createResolveAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usage,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = VK_SAMPLE_COUNT_1_BIT,  // Resolve attachments are always single-sampled
        .usage = usage == 0 ? getDefaultUsageFlags(AttachmentType::Resolve) : usage,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::Resolve,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    };
}

AttachmentSpec AttachmentFactory::createSwapchainColorAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,  // Swapchain images are color attachments
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::Color,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    };
}

AttachmentSpec AttachmentFactory::createMSAAColorAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples
) {
    return createColorAttachment(
        format,
        extent,
        samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE  // MSAA attachments typically don't need to be stored
    );
}

AttachmentSpec AttachmentFactory::createMSAADepthAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples
) {
    return createDepthAttachment(
        format,
        extent,
        samples,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE  // MSAA depth typically doesn't need to be stored
    );
}

AttachmentSpec AttachmentFactory::createInputAttachment(
    VkFormat format,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout
) {
    return AttachmentSpec{
        .format = format,
        .extent = extent,
        .samples = samples,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        .initialLayout = initialLayout,
        .finalLayout = finalLayout,
        .type = AttachmentType::Color,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,  // Input attachments typically need to load existing data
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
    };
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