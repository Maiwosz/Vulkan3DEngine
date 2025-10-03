#include "RenderGraphManager.h"
#include "RenderNodeManager.h"
#include "RenderGraphTemplate.h"
#include "SwapChain.h"
#include <stdexcept>
#include "EngineCore.h"

RenderGraphManager::RenderGraphManager(EngineCore& engineCore)
	: m_engineCore(engineCore)
    , m_nodeManager(engineCore.renderNodeManager())
    , m_renderPassManager(engineCore.renderPassManager())
    , m_attachmentManager(engineCore.attachmentManager())
    , m_framebufferManager(engineCore.framebufferManager())
    , m_swapchain(engineCore.swapChain()) {
}

RenderGraphManager::~RenderGraphManager() {
    // Resources will be automatically destroyed when maps are destroyed
}

RenderGraph* RenderGraphManager::getResource(RenderGraphHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end() && !it->second.markedForDeletion) {
        return it->second.graph.get();
    }
    return nullptr;
}

bool RenderGraphManager::isValid(RenderGraphHandle handle) const {
    auto it = m_resources.find(handle);
    return it != m_resources.end() && !it->second.markedForDeletion;
}

void RenderGraphManager::releaseResource(RenderGraphHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end()) {
        it->second.markedForDeletion = true;

        // If no references, actually remove it
        if (it->second.referenceCount == 0) {
            // Remove from cache first
            for (auto& [typeIndex, typeCache] : m_graphCache) {
                for (auto cacheIt = typeCache.begin(); cacheIt != typeCache.end(); ++cacheIt) {
                    if (cacheIt->second == handle) {
                        typeCache.erase(cacheIt);
                        break;
                    }
                }
                if (typeCache.empty()) {
                    m_graphCache.erase(typeIndex);
                    break;
                }
            }
            m_resources.erase(it);
        }
    }
}

void RenderGraphManager::addReference(RenderGraphHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end()) {
        it->second.referenceCount++;
        it->second.markedForDeletion = false; // Un-mark for deletion if referenced again
    }
}

void RenderGraphManager::removeReference(RenderGraphHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end() && it->second.referenceCount > 0) {
        it->second.referenceCount--;

        // If marked for deletion and no references, remove it
        if (it->second.referenceCount == 0 && it->second.markedForDeletion) {
            // Remove from cache first
            for (auto& [typeIndex, typeCache] : m_graphCache) {
                for (auto cacheIt = typeCache.begin(); cacheIt != typeCache.end(); ++cacheIt) {
                    if (cacheIt->second == handle) {
                        typeCache.erase(cacheIt);
                        break;
                    }
                }
                if (typeCache.empty()) {
                    m_graphCache.erase(typeIndex);
                    break;
                }
            }
            m_resources.erase(it);
        }
    }
}

void RenderGraphManager::clearCache() {
    // Mark all cached graphs for deletion
    for (const auto& [typeIndex, typeCache] : m_graphCache) {
        for (const auto& [keyHash, handle] : typeCache) {
            auto resourceIt = m_resources.find(handle);
            if (resourceIt != m_resources.end()) {
                resourceIt->second.markedForDeletion = true;

                // If no references, remove immediately
                if (resourceIt->second.referenceCount == 0) {
                    m_resources.erase(resourceIt);
                }
            }
        }
    }

    m_graphCache.clear();
}

size_t RenderGraphManager::getCacheSize() const {
    size_t total = 0;
    for (const auto& [typeIndex, typeCache] : m_graphCache) {
        total += typeCache.size();
    }
    return total;
}

VkExtent2D RenderGraphManager::extractDimensions(const RenderTarget& target) const {
    if (target.isSwapchain()) {
        return m_swapchain.getSwapChainExtent();
    }
    else if (target.isTexture()) {
        // RenderTarget handles texture dimension extraction internally
        return target.getDimensions(&m_swapchain);
    }
    else {
        throw std::runtime_error("Unknown render target type");
    }
}

RenderGraphHandle RenderGraphManager::generateHandle() {
    return RenderGraphHandle(m_nextHandleId++);
}

SmartHandle<RenderGraphHandle, RenderGraph> RenderGraphManager::createSmartHandle(RenderGraphHandle handle) {
    return makeSmartHandle(handle);
}