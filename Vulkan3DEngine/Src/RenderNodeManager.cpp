#include "RenderNodeManager.h"
#include "RenderNodeTemplate.h"
#include "SwapChain.h"
#include <stdexcept>
#include "EngineCore.h"

RenderNodeManager::RenderNodeManager(EngineCore& engineCore)
	: m_engineCore(engineCore)
    , m_renderPassManager(engineCore.renderPassManager())
    , m_attachmentManager(engineCore.attachmentManager())
    , m_swapChain(engineCore.swapChain()) {
}

RenderNodeManager::~RenderNodeManager() {
    // Resources will be automatically destroyed when map is destroyed
    // No need to manually clean up since we own all resources
}

RenderNode* RenderNodeManager::getResource(RenderNodeHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end()) {
        return it->second.node.get();
    }
    return nullptr;
}

bool RenderNodeManager::isValid(RenderNodeHandle handle) const {
    auto it = m_resources.find(handle);
    return it != m_resources.end() && !it->second.markedForDeletion;
}

void RenderNodeManager::releaseResource(RenderNodeHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end()) {
        it->second.markedForDeletion = true;

        // If no references, actually remove it
        if (it->second.referenceCount == 0) {
            // Remove from all type caches
            for (auto& [typeIndex, typeCache] : m_nodeCache) {
                for (auto cacheIt = typeCache.begin(); cacheIt != typeCache.end(); ++cacheIt) {
                    if (cacheIt->second == handle) {
                        typeCache.erase(cacheIt);
                        break;
                    }
                }
            }

            m_resources.erase(it);
        }
    }
}

void RenderNodeManager::addReference(RenderNodeHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end()) {
        it->second.referenceCount++;
        it->second.markedForDeletion = false; // Un-mark for deletion if referenced again
    }
}

void RenderNodeManager::removeReference(RenderNodeHandle handle) {
    auto it = m_resources.find(handle);
    if (it != m_resources.end() && it->second.referenceCount > 0) {
        it->second.referenceCount--;

        // If marked for deletion and no references, remove it
        if (it->second.referenceCount == 0 && it->second.markedForDeletion) {
            // Remove from all type caches
            for (auto& [typeIndex, typeCache] : m_nodeCache) {
                for (auto cacheIt = typeCache.begin(); cacheIt != typeCache.end(); ++cacheIt) {
                    if (cacheIt->second == handle) {
                        typeCache.erase(cacheIt);
                        break;
                    }
                }
            }

            m_resources.erase(it);
        }
    }
}

void RenderNodeManager::clearCache() {
    // Mark all cached nodes for deletion
    for (const auto& [typeIndex, typeCache] : m_nodeCache) {
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

    m_nodeCache.clear();
}

size_t RenderNodeManager::getCacheSize() const {
    size_t totalSize = 0;
    for (const auto& [typeIndex, typeCache] : m_nodeCache) {
        totalSize += typeCache.size();
    }
    return totalSize;
}

RenderNodeHandle RenderNodeManager::generateHandle() {
    return RenderNodeHandle(m_nextHandleId++);
}

SmartHandle<RenderNodeHandle, RenderNode> RenderNodeManager::createSmartHandle(RenderNodeHandle handle) {
    return makeSmartHandle(handle);
}