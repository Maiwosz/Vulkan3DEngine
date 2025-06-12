#include "FrameBufferManager.h"
#include "LogicalDevice.h"

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
    RenderPassManager& renderPassManager,
    AttachmentManager& attachmentManager)
    : m_device(logicalDevice),
    m_renderPassManager(renderPassManager),
    m_attachmentManager(attachmentManager) {
}

FrameBufferManager::~FrameBufferManager() {
    // Destroy all framebuffers
    for (const auto& [handle, resource] : m_frameBuffers) {
        if (resource->frameBuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.get(), resource->frameBuffer, nullptr);
        }
    }

    m_frameBuffers.clear();
    m_configToHandle.clear();
    m_availablePool.clear();
    m_sizeDependent.clear();
}

// IResourceManager interface implementation
FrameBufferResource* FrameBufferManager::getResource(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    return (it != m_frameBuffers.end()) ? it->second.get() : nullptr;
}

bool FrameBufferManager::isValid(FrameBufferHandle handle) const {
    return m_frameBuffers.find(handle) != m_frameBuffers.end();
}

void FrameBufferManager::releaseResource(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        // Usuń z aktywnego mapowania config->handle
        m_configToHandle.erase(it->second->config);

        // Dodaj do puli dostępnych zasobów
        m_availablePool[it->second->config].push_back(handle);

        // Wyzeruj licznik referencji
        it->second->refCount = 0;

        // NIE usuwamy z m_frameBuffers - zasób pozostaje w pamięci
        SPDLOG_DEBUG("Released framebuffer handle {} to pool", handle.id);
    }
}

void FrameBufferManager::addReference(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        it->second->refCount++;
        SPDLOG_DEBUG("Added reference to framebuffer handle {}, refCount: {}",
            handle.id, it->second->refCount);
    }
}

void FrameBufferManager::removeReference(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        if (it->second->refCount > 0) {
            it->second->refCount--;
            SPDLOG_DEBUG("Removed reference from framebuffer handle {}, refCount: {}",
                handle.id, it->second->refCount);

            // Automatycznie zwolnij zasób gdy refCount osiągnie 0
            if (it->second->refCount == 0) {
                SPDLOG_DEBUG("RefCount reached 0, releasing framebuffer handle {}", handle.id);
                releaseResource(handle);
            }
        }
    }
}

// Original FrameBufferManager interface
FrameBufferHandle FrameBufferManager::acquireFrameBuffer(const FrameBufferConfig& config) {
    // Sprawdź czy mamy aktywny framebuffer o tej konfiguracji
    auto activeIt = m_configToHandle.find(config);
    if (activeIt != m_configToHandle.end()) {
        // Zwiększ licznik referencji dla istniejącego framebuffera
        addReference(activeIt->second);
        SPDLOG_DEBUG("Reusing active framebuffer handle {}", activeIt->second.id);
        return activeIt->second;
    }

    // Sprawdź czy mamy dostępny framebuffer w puli
    auto poolIt = m_availablePool.find(config);
    if (poolIt != m_availablePool.end() && !poolIt->second.empty()) {
        // Pobierz handle z puli
        FrameBufferHandle handle = poolIt->second.back();
        poolIt->second.pop_back();

        // Jeśli pula jest pusta, usuń wpis
        if (poolIt->second.empty()) {
            m_availablePool.erase(poolIt);
        }

        // Przywróć mapowanie config->handle
        m_configToHandle[config] = handle;

        // Zwiększ licznik referencji
        addReference(handle);

        SPDLOG_DEBUG("Reusing framebuffer handle {} from pool", handle.id);
        return handle;
    }

    // Jeśli nie ma w puli, utwórz nowy
    FrameBufferHandle newHandle = createFrameBuffer(config);
    if (newHandle.id != 0) {
        // Zwiększ licznik referencji dla nowo utworzonego framebuffera
        addReference(newHandle);
        SPDLOG_DEBUG("Created new framebuffer handle {}", newHandle.id);
    }
    return newHandle;
}

FrameBufferHandle FrameBufferManager::acquireFrameBuffer(
    RenderPassHandle renderPassHandle,
    const std::vector<AttachmentHandle>& attachmentHandles,
    VkExtent2D extent) {

    FrameBufferConfig config{
        .renderPassHandle = renderPassHandle,
        .attachmentHandles = attachmentHandles,
        .extent = extent
    };

    return acquireFrameBuffer(config);
}

void FrameBufferManager::onResize(VkExtent2D newExtent) {
    SPDLOG_INFO("Handling framebuffer resize to {}x{}", newExtent.width, newExtent.height);

    // Zbierz handles do zwolnienia - tylko te które są size-dependent
    std::vector<FrameBufferHandle> toRelease;

    // Zwolnij aktywne size-dependent framebuffery
    for (const auto& handle : m_sizeDependent) {
        auto it = m_frameBuffers.find(handle);
        if (it != m_frameBuffers.end()) {
            // Sprawdź czy framebuffer jest aktualnie używany (ma aktywne mapowanie)
            auto configIt = m_configToHandle.find(it->second->config);
            if (configIt != m_configToHandle.end() && configIt->second == handle) {
                toRelease.push_back(handle);
            }
        }
    }

    // Zwolnij aktywne framebuffery (wracają do puli)
    for (const auto& handle : toRelease) {
        SPDLOG_DEBUG("Releasing size-dependent framebuffer handle {} due to resize", handle.id);
        releaseResource(handle);
    }

    // Wyczyść pule size-dependent framebufferów (faktycznie je zniszcz)
    for (auto poolIt = m_availablePool.begin(); poolIt != m_availablePool.end();) {
        // Sprawdź czy config jest zależny od rozmiaru - jeśli używa obecnych wymiarów swapchain
        bool isResizeDependent = true; // Załóż że wszystkie są zależne od rozmiaru

        if (isResizeDependent) {
            SPDLOG_DEBUG("Destroying {} framebuffers from pool due to resize", poolIt->second.size());

            // Zniszcz wszystkie framebuffery z tej puli
            for (const auto& handle : poolIt->second) {
                auto fbIt = m_frameBuffers.find(handle);
                if (fbIt != m_frameBuffers.end()) {
                    if (fbIt->second->frameBuffer != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(m_device.get(), fbIt->second->frameBuffer, nullptr);
                    }
                    m_frameBuffers.erase(fbIt);
                }
            }
            poolIt = m_availablePool.erase(poolIt);
        }
        else {
            ++poolIt;
        }
    }

    m_sizeDependent.clear();
    SPDLOG_INFO("Framebuffer resize handling completed");
}

FrameBufferHandle FrameBufferManager::createFrameBuffer(const FrameBufferConfig& config) {
    // Verify the render pass is valid
    VkRenderPass renderPass = *m_renderPassManager.getResource(config.renderPassHandle);
    if (renderPass == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Invalid render pass handle: {}", config.renderPassHandle.id);
        return FrameBufferHandle(0);
    }

    // Collect image views for the attachments
    std::vector<VkImageView> attachmentViews;
    attachmentViews.reserve(config.attachmentHandles.size());

    for (const auto& attachHandle : config.attachmentHandles) {
        const Attachment* attachment = m_attachmentManager.getResource(attachHandle);
        if (!attachment) {
            SPDLOG_ERROR("Invalid attachment handle: {}", attachHandle.id);
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
        SPDLOG_ERROR("Failed to create framebuffer");
        return FrameBufferHandle(0);
    }

    // Create handle and register framebuffer
    FrameBufferHandle handle(m_nextHandleId++);

    auto resource = std::make_unique<FrameBufferResource>(framebuffer, config);
    m_frameBuffers[handle] = std::move(resource);
    m_configToHandle[config] = handle;

    // Track size-dependent framebuffers
    m_sizeDependent.push_back(handle);

    SPDLOG_DEBUG("Created framebuffer handle {} ({}x{})", handle.id, config.extent.width, config.extent.height);
    return handle;
}

void FrameBufferManager::destroyFrameBuffer(FrameBufferHandle handle) {
    auto it = m_frameBuffers.find(handle);
    if (it != m_frameBuffers.end()) {
        // Remove from size-dependent list if present
        auto sizeDependentIt = std::find(m_sizeDependent.begin(), m_sizeDependent.end(), handle);
        if (sizeDependentIt != m_sizeDependent.end()) {
            m_sizeDependent.erase(sizeDependentIt);
        }

        // Remove from config to handle map
        m_configToHandle.erase(it->second->config);

        // Remove from available pool if present
        auto& config = it->second->config;
        auto poolIt = m_availablePool.find(config);
        if (poolIt != m_availablePool.end()) {
            auto handleIt = std::find(poolIt->second.begin(), poolIt->second.end(), handle);
            if (handleIt != poolIt->second.end()) {
                poolIt->second.erase(handleIt);
                if (poolIt->second.empty()) {
                    m_availablePool.erase(poolIt);
                }
            }
        }

        // Destroy the framebuffer
        if (it->second->frameBuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.get(), it->second->frameBuffer, nullptr);
        }

        // Remove from handle map
        m_frameBuffers.erase(it);

        SPDLOG_DEBUG("Destroyed framebuffer handle {}", handle.id);
    }
}