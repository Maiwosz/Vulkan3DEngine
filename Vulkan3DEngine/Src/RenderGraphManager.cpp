#include "RenderGraphManager.h"
#include "RenderGraphTemplate.h"
#include "RenderTarget.h"
#include <stdexcept>

RenderGraphManager::RenderGraphManager(
    AttachmentManager& attachmentMgr,
    RenderPassManager& renderPassMgr)
    : m_attachmentManager(attachmentMgr)
    , m_renderPassManager(renderPassMgr) {
}

RenderGraphManager::~RenderGraphManager() {
    m_cache.clear();
    m_renderGraphs.clear();
}

RenderGraphHandle RenderGraphManager::acquireRenderGraph(
    const SmartRenderGraphTemplateHandle& graphTemplate,
    const RenderTarget& target) {

    if (!graphTemplate.isValid()) {
        throw std::runtime_error("RenderGraphManager: invalid template handle");
    }

    RenderGraphCacheKey key(graphTemplate.handle(), target);

    // Check cache first
    auto cacheIt = m_cache.find(key);
    if (cacheIt != m_cache.end()) {
        RenderGraphHandle handle = cacheIt->second;
        addReference(handle);
        return handle;
    }

    // Create new render graph
    return createRenderGraph(graphTemplate, target);
}

SmartRenderGraphHandle RenderGraphManager::acquireSmartRenderGraph(
    const SmartRenderGraphTemplateHandle& graphTemplate,
    const RenderTarget& target) {

    RenderGraphHandle handle = acquireRenderGraph(graphTemplate, target);
    return makeSmartHandle(handle);
}

RenderGraphHandle RenderGraphManager::createRenderGraph(
    const SmartRenderGraphTemplateHandle& graphTemplate,
    const RenderTarget& target) {

    if (!graphTemplate.isValid()) {
        throw std::runtime_error("RenderGraphManager: invalid template handle");
    }

    // Get template through smart handle - uses internal manager pointer
    const RenderGraphTemplate* tmpl = graphTemplate.get();
    if (!tmpl) {
        throw std::runtime_error("RenderGraphManager: failed to get template");
    }

    // Create the concrete render graph
    auto graph = tmpl->createRenderGraph(
        target,
        m_attachmentManager,
        m_renderPassManager);

    if (!graph || !graph->isValid()) {
        throw std::runtime_error("RenderGraphManager: failed to create valid render graph");
    }

    // Allocate handle
    RenderGraphHandle handle(m_nextHandleId);
    ++m_nextHandleId;

    // Store in cache
    RenderGraphCacheKey key(graphTemplate.handle(), target);
    m_cache[key] = handle;

    // Store entry with a copy of the smart handle to keep template alive
    m_renderGraphs.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(handle),
        std::forward_as_tuple(
            std::move(graph),
            key,
            graphTemplate,  // Copy smart handle - keeps template alive
            1)
    );

    return handle;
}

void RenderGraphManager::invalidateTarget(const RenderTarget& target) {
    std::vector<RenderGraphHandle> toRemove;

    // Find all graphs for this target
    for (const auto& [handle, entry] : m_renderGraphs) {
        if (entry.cacheKey.target == target) {
            toRemove.push_back(handle);
        }
    }

    // Remove from cache and storage
    for (RenderGraphHandle handle : toRemove) {
        removeFromCache(handle);
        m_renderGraphs.erase(handle);
    }
}

void RenderGraphManager::invalidateTemplate(RenderGraphTemplateHandle templateHandle) {
    std::vector<RenderGraphHandle> toRemove;

    // Find all graphs using this template
    for (const auto& [handle, entry] : m_renderGraphs) {
        if (entry.cacheKey.templateHandle == templateHandle) {
            toRemove.push_back(handle);
        }
    }

    // Remove from cache and storage
    for (RenderGraphHandle handle : toRemove) {
        removeFromCache(handle);
        m_renderGraphs.erase(handle);
    }
}

void RenderGraphManager::invalidateAll() {
    m_cache.clear();
    m_renderGraphs.clear();
}

void RenderGraphManager::removeFromCache(RenderGraphHandle handle) {
    auto it = m_renderGraphs.find(handle);
    if (it != m_renderGraphs.end()) {
        m_cache.erase(it->second.cacheKey);
    }
}

bool RenderGraphManager::rebuildRenderGraph(RenderGraphHandle handle) {
    auto it = m_renderGraphs.find(handle);
    if (it == m_renderGraphs.end()) {
        return false;
    }

    RenderGraphEntry& entry = it->second;

    try {
        // Get template through stored smart handle
        const RenderGraphTemplate* tmpl = entry.templateHandle.get();
        if (!tmpl) {
            return false;
        }

        // Create new render graph with same template and target
        auto newGraph = tmpl->createRenderGraph(
            entry.cacheKey.target,
            m_attachmentManager,
            m_renderPassManager);

        if (!newGraph || !newGraph->isValid()) {
            return false;
        }

        // Replace the graph while keeping handle and reference count
        entry.graph = std::move(newGraph);

        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

RenderGraph* RenderGraphManager::getResource(RenderGraphHandle handle) {
    auto it = m_renderGraphs.find(handle);
    if (it == m_renderGraphs.end()) {
        return nullptr;
    }
    return it->second.graph.get();
}

bool RenderGraphManager::isValid(RenderGraphHandle handle) const {
    return m_renderGraphs.find(handle) != m_renderGraphs.end();
}

void RenderGraphManager::releaseResource(RenderGraphHandle handle) {
    auto it = m_renderGraphs.find(handle);
    if (it == m_renderGraphs.end()) {
        return;
    }

    if (it->second.referenceCount > 0) {
        --it->second.referenceCount;
    }

    // Remove if no more references
    if (it->second.referenceCount == 0) {
        removeFromCache(handle);
        m_renderGraphs.erase(it);
    }
}

void RenderGraphManager::addReference(RenderGraphHandle handle) {
    auto it = m_renderGraphs.find(handle);
    if (it != m_renderGraphs.end()) {
        ++it->second.referenceCount;
    }
}

void RenderGraphManager::removeReference(RenderGraphHandle handle) {
    releaseResource(handle);
}