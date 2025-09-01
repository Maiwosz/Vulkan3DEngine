#include "RenderPassManager.h"
#include "LogicalDevice.h"
#include <stdexcept>
#include <algorithm>
#include <functional>

// Hash function implementation for RenderPassConfig
size_t RenderPassConfig::hash() const {
    size_t h = std::hash<size_t>()(attachments.size());

    // Hash each attachment specification
    for (const auto& attachment : attachments) {
        h ^= std::hash<AttachmentSpec>()(attachment);
    }

    // Hash subpass configuration
    for (const auto& index : colorAttachmentIndices) {
        h ^= std::hash<uint32_t>()(index) << 1;
    }

    h ^= std::hash<uint32_t>()(depthAttachmentIndex) << 2;
    h ^= std::hash<uint32_t>()(resolveAttachmentIndex) << 3;

    return h;
}

// Equality operator implementation for RenderPassConfig
bool RenderPassConfig::operator==(const RenderPassConfig& other) const {
    return attachments == other.attachments &&
        colorAttachmentIndices == other.colorAttachmentIndices &&
        depthAttachmentIndex == other.depthAttachmentIndex &&
        resolveAttachmentIndex == other.resolveAttachmentIndex;
}

RenderPassManager::RenderPassManager(const LogicalDevice& logicalDevice)
    : m_device(logicalDevice)
{
}

RenderPassManager::~RenderPassManager() {
    for (const auto& [handle, entry] : m_renderPasses) {
        vkDestroyRenderPass(m_device.get(), entry.renderPass, nullptr);
    }

    m_renderPasses.clear();
    m_configToHandle.clear();
}

RenderPassHandle RenderPassManager::acquireRenderPass(const RenderPassConfig& config) {
    // Check if we already have a render pass with this configuration
    auto configIt = m_configToHandle.find(config);
    if (configIt != m_configToHandle.end()) {
        return configIt->second;
    }

    // Create a new render pass
    return createRenderPass(config);
}

void RenderPassManager::recreateRenderPass(RenderPassHandle handle, const RenderPassConfig& newConfig) {
    if (!isValid(handle)) {
        throw std::runtime_error("Invalid render pass handle for recreation");
    }

    auto it = m_renderPasses.find(handle);
    if (it == m_renderPasses.end()) {
        throw std::runtime_error("Render pass handle not found");
    }

    // Remove old config mapping
    auto configIt = m_configToHandle.find(it->second.config);
    if (configIt != m_configToHandle.end()) {
        m_configToHandle.erase(configIt);
    }

    // Destroy old render pass
    vkDestroyRenderPass(m_device.get(), it->second.renderPass, nullptr);

    // Create new render pass with new configuration
    VkRenderPass newRenderPass = createVkRenderPass(newConfig);

    // Update stored data
    it->second.renderPass = newRenderPass;
    it->second.config = newConfig;
    m_configToHandle[newConfig] = handle;
}

VkRenderPass* RenderPassManager::getResource(RenderPassHandle handle) {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_renderPasses.find(handle);
    if (it == m_renderPasses.end()) {
        return nullptr;
    }

    return &it->second.renderPass;
}

bool RenderPassManager::isValid(RenderPassHandle handle) const {
    if (!handle.isValid()) {
        return false;
    }

    return m_renderPasses.find(handle) != m_renderPasses.end();
}

void RenderPassManager::releaseResource(RenderPassHandle handle) {
    // No-op - render passes are kept until manager destruction
    (void)handle; // Suppress unused parameter warning
}

void RenderPassManager::addReference(RenderPassHandle handle) {
    // No-op - render passes are shared resources, no reference counting needed
    // This manager keeps all render passes until destruction
    (void)handle; // Suppress unused parameter warning
}

void RenderPassManager::removeReference(RenderPassHandle handle) {
    // No-op - render passes are shared resources, no reference counting needed
    // This manager keeps all render passes until destruction
    (void)handle; // Suppress unused parameter warning
}

RenderPassHandle RenderPassManager::createRenderPass(const RenderPassConfig& config) {
    // Create the Vulkan render pass using helper function
    VkRenderPass renderPass = createVkRenderPass(config);

    // Create a new handle
    RenderPassHandle newHandle(m_nextHandleId++);

    // Store the render pass
    RenderPassEntry entry{ renderPass, config };
    m_renderPasses[newHandle] = entry;
    m_configToHandle[config] = newHandle;

    return newHandle;
}

VkRenderPass RenderPassManager::createVkRenderPass(const RenderPassConfig& config) {
    // Verify we have at least one attachment
    if (config.attachments.empty()) {
        throw std::runtime_error("Cannot create render pass with no attachments");
    }

    // Convert our config to Vulkan structures
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.reserve(config.attachments.size());

    for (const auto& attachment : config.attachments) {
        VkAttachmentDescription desc{};
        desc.format = attachment.format;
        desc.samples = attachment.samples;
        desc.loadOp = attachment.loadOp;
        desc.storeOp = attachment.storeOp;
        desc.stencilLoadOp = attachment.stencilLoadOp;
        desc.stencilStoreOp = attachment.stencilStoreOp;
        desc.initialLayout = attachment.initialLayout;
        desc.finalLayout = attachment.finalLayout;

        attachmentDescriptions.push_back(desc);
    }

    // Set up the subpass - we support one subpass for now
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Color attachments
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    colorAttachmentRefs.reserve(config.colorAttachmentIndices.size());

    for (uint32_t index : config.colorAttachmentIndices) {
        if (index >= config.attachments.size()) {
            throw std::runtime_error("Color attachment index out of bounds");
        }

        VkAttachmentReference ref{};
        ref.attachment = index;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentRefs.push_back(ref);
    }

    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();

    // Depth attachment (optional)
    VkAttachmentReference depthAttachmentRef{};
    if (config.depthAttachmentIndex != UINT32_MAX) {
        if (config.depthAttachmentIndex >= config.attachments.size()) {
            throw std::runtime_error("Depth attachment index out of bounds");
        }

        depthAttachmentRef.attachment = config.depthAttachmentIndex;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }

    // Resolve attachment (optional)
    VkAttachmentReference resolveAttachmentRef{};
    if (config.resolveAttachmentIndex != UINT32_MAX) {
        if (config.resolveAttachmentIndex >= config.attachments.size()) {
            throw std::runtime_error("Resolve attachment index out of bounds");
        }

        resolveAttachmentRef.attachment = config.resolveAttachmentIndex;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.pResolveAttachments = &resolveAttachmentRef;
    }

    // Set up dependencies
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Create the render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(m_device.get(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    return renderPass;
}