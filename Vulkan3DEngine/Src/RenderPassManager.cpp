#include "RenderPassManager.h"
#include "LogicalDevice.h"
#include <stdexcept>
#include <algorithm>
#include <functional>

// Hash function implementation for RenderPassConfig
size_t RenderPassConfig::hash() const {
    size_t h = std::hash<size_t>()(attachments.size());

    // Hash each attachment description
    for (const auto& attachment : attachments) {
        h ^= std::hash<uint32_t>()(static_cast<uint32_t>(attachment.format)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.samples)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.loadOp)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.storeOp)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.initialLayout)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.finalLayout)) +
            std::hash<uint32_t>()(static_cast<uint32_t>(attachment.type));
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
    cleanup();
}

RenderPassHandle RenderPassManager::getOrCreate(const RenderPassConfig& config) {
    // Check if we already have a render pass with this configuration
    auto configIt = m_configToHandle.find(config);
    if (configIt != m_configToHandle.end()) {
        return configIt->second;
    }

    // Create a new render pass
    return createRenderPass(config);
}

VkRenderPass RenderPassManager::get(RenderPassHandle handle) const {
    auto it = m_renderPasses.find(handle);
    if (it == m_renderPasses.end()) {
        throw std::runtime_error("Invalid render pass handle: " + std::to_string(handle.id));
    }
    return it->second.renderPass;
}

bool RenderPassManager::isValid(RenderPassHandle handle) const {
    return m_renderPasses.find(handle) != m_renderPasses.end();
}

void RenderPassManager::destroy(RenderPassHandle handle) {
    auto it = m_renderPasses.find(handle);
    if (it == m_renderPasses.end()) {
        return;
    }

    // Remove from config map
    m_configToHandle.erase(it->second.config);

    // Destroy the VkRenderPass
    vkDestroyRenderPass(m_device.get(), it->second.renderPass, nullptr);

    // Remove from handle map
    m_renderPasses.erase(it);
}

void RenderPassManager::cleanup() {
    for (const auto& [handle, entry] : m_renderPasses) {
        vkDestroyRenderPass(m_device.get(), entry.renderPass, nullptr);
    }

    m_renderPasses.clear();
    m_configToHandle.clear();
}

RenderPassHandle RenderPassManager::createRenderPass(const RenderPassConfig& config) {
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
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = attachment.initialLayout;
        desc.finalLayout = attachment.finalLayout;

        if (attachment.type == AttachmentType::Depth) {
            desc.stencilLoadOp = attachment.loadOp;
            desc.stencilStoreOp = attachment.storeOp;
        }

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

    // Create a new handle
    RenderPassHandle newHandle(m_nextHandleId++);

    // Store the render pass
    RenderPassEntry entry{ renderPass, config };
    m_renderPasses[newHandle] = entry;
    m_configToHandle[config] = newHandle;

    return newHandle;
}