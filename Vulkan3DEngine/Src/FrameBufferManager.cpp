#include "FrameBufferManager.h"
#include "LogicalDevice.h" // Assuming this exists based on your forward declarations

// FrameBufferConfig implementation
size_t FrameBufferConfig::hash() const {
    size_t h = std::hash<uint32_t>()(renderPassHandle.id);

    // Combine hashes of all attachment handles
    for (const auto& attachHandle : attachmentHandles) {
        h ^= std::hash<uint32_t>()(attachHandle.id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }

    // Combine with extent dimensions
    h ^= std::hash<uint32_t>()(extent.width) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>()(extent.height) + 0x9e3779b9 + (h << 6) + (h >> 2);

    return h;
}

bool FrameBufferConfig::operator==(const FrameBufferConfig& other) const {
    return renderPassHandle.id == other.renderPassHandle.id &&
        attachmentHandles == other.attachmentHandles &&
        extent.width == other.extent.width &&
        extent.height == other.extent.height;
}

// FrameBufferManager implementation
FrameBufferManager::FrameBufferManager(
    const LogicalDevice& logicalDevice,
    const RenderPassManager& renderPassManager,
    const AttachmentManager& attachmentManager)
    : m_device(logicalDevice),
    m_renderPassManager(renderPassManager),
    m_attachmentManager(attachmentManager) {
}

FrameBufferManager::~FrameBufferManager() {
    cleanup();
}

FrameBufferHandle FrameBufferManager::getOrCreate(const FrameBufferConfig& config) {
    // Check if we already have this configuration
    auto it = m_configToHandle.find(config);
    if (it != m_configToHandle.end()) {
        return it->second;
    }

    // Otherwise create a new framebuffer
    return createFrameBuffer(config);
}

FrameBufferHandle FrameBufferManager::getOrCreate(
    RenderPassHandle renderPassHandle,
    const std::vector<AttachmentHandle>& attachmentHandles,
    VkExtent2D extent) {

    FrameBufferConfig config{
        .renderPassHandle = renderPassHandle,
        .attachmentHandles = attachmentHandles,
        .extent = extent
    };

    return getOrCreate(config);
}

VkFramebuffer FrameBufferManager::get(FrameBufferHandle handle) const {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        return it->second.frameBuffer;
    }
    return VK_NULL_HANDLE;
}

bool FrameBufferManager::isValid(FrameBufferHandle handle) const {
    return m_frameBuffers.find(handle) != m_frameBuffers.end();
}

void FrameBufferManager::destroy(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        // Remove from size-dependent list if present
        auto sizeDependentIt = std::find(m_sizeDependent.begin(), m_sizeDependent.end(), handle);
        if (sizeDependentIt != m_sizeDependent.end()) {
            m_sizeDependent.erase(sizeDependentIt);
        }

        // Remove from config to handle map
        m_configToHandle.erase(it->second.config);

        // Destroy the framebuffer
        vkDestroyFramebuffer(m_device.get(), it->second.frameBuffer, nullptr);

        // Remove from handle map
        m_frameBuffers.erase(it);
    }
}

void FrameBufferManager::onResize(VkExtent2D newExtent) {
    // Collect handles to destroy (can't modify while iterating)
    std::vector<FrameBufferHandle> toDestroy;

    for (const auto& handle : m_sizeDependent) {
        auto it = m_frameBuffers.find(handle);
        if (it != m_frameBuffers.end()) {
            toDestroy.push_back(handle);
        }
    }

    // Destroy all size-dependent framebuffers
    for (const auto& handle : toDestroy) {
        destroy(handle);
    }

    m_sizeDependent.clear();
}

void FrameBufferManager::cleanup() {
    // Destroy all framebuffers
    for (const auto& [handle, entry] : m_frameBuffers) {
        vkDestroyFramebuffer(m_device.get(), entry.frameBuffer, nullptr);
    }

    m_frameBuffers.clear();
    m_configToHandle.clear();
    m_sizeDependent.clear();
}

FrameBufferHandle FrameBufferManager::createFrameBuffer(const FrameBufferConfig& config) {
    // Verify the render pass is valid
    VkRenderPass renderPass = m_renderPassManager.getRenderPass(config.renderPassHandle);
    if (renderPass == VK_NULL_HANDLE) {
        // Invalid render pass
        return FrameBufferHandle(0);
    }

    // Collect image views for the attachments
    std::vector<VkImageView> attachmentViews;
    attachmentViews.reserve(config.attachmentHandles.size());

    for (const auto& attachHandle : config.attachmentHandles) {
        const Attachment* attachment = m_attachmentManager.get(attachHandle);
        if (!attachment) {
            // Invalid attachment
            return FrameBufferHandle(0);
        }
        attachmentViews.push_back(attachment->getImageView());
    }

    // Create the framebuffer
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
    framebufferInfo.pAttachments = attachmentViews.data();
    framebufferInfo.width = config.extent.width;
    framebufferInfo.height = config.extent.height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(m_device.get(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        // Failed to create framebuffer
        return FrameBufferHandle(0);
    }

    // Create handle and register framebuffer
    FrameBufferHandle handle(m_nextHandleId++);

    FrameBufferEntry entry{
        .frameBuffer = framebuffer,
        .config = config
    };

    m_frameBuffers[handle] = entry;
    m_configToHandle[config] = handle;

    // Track size-dependent framebuffers
    m_sizeDependent.push_back(handle);

    return handle;
}