#include "RenderGraphTemplateManager.h"
#include "BuiltinGraphTemplates.h"
#include "AssetManager.h"
#include <stdexcept>

RenderGraphTemplateManager::RenderGraphTemplateManager() {
    initializeBuiltInTemplates();
}

void RenderGraphTemplateManager::initializeBuiltInTemplates() {
    // Register all built-in templates

    // Forward rendering template
    auto forwardTemplate = BuiltinGraphTemplates::createForwardRendering();
    registerTemplate(std::move(forwardTemplate), "", true);

    // Add more built-in templates here as needed
    // Example:
    // auto deferredTemplate = BuiltinGraphTemplates::createDeferredRendering();
    // registerTemplate(std::move(deferredTemplate), "", true);
}

RenderGraphTemplateHandle RenderGraphTemplateManager::registerTemplate(
    std::unique_ptr<RenderGraphTemplate> tmpl,
    const std::string& filename,
    bool isBuiltIn) {

    if (!tmpl || !tmpl->validate()) {
        throw std::runtime_error("RenderGraphTemplateManager: Invalid template");
    }

    // Generate new handle
    RenderGraphTemplateHandle handle(m_nextHandle.id++);

    // Calculate memory usage
    uint64_t memSize = estimateTemplateSize(*tmpl);

    // Get template name
    std::string name = tmpl->getName();

    // Create cached entry
    CachedTemplate cached;
    cached.template_ = std::move(tmpl);
    cached.memorySize = memSize;
    cached.handle = handle;
    cached.name = name;
    cached.isBuiltIn = isBuiltIn;

    // Store in appropriate map
    if (isBuiltIn) {
        // Built-in templates don't have filenames, use name as key
        auto [it, inserted] = m_templatesByFilename.emplace(name, std::move(cached));

        // Register in lookup maps
        m_templatesByName[name] = handle;
        m_templatesByHandle[handle] = &it->second;
    }
    else {
        // File-based templates use filename as key
        auto [it, inserted] = m_templatesByFilename.emplace(filename, std::move(cached));

        // Register in lookup maps
        m_templatesByName[name] = handle;
        m_templatesByHandle[handle] = &it->second;
    }

    return handle;
}

bool RenderGraphTemplateManager::prepareAsset(
    const AssetHandle& handle,
    const AssetLib::AssetData& data,
    AssetManager& manager) {

    // Verify asset type
    if (data.header.assetType != AssetType::RenderGraph) {
        return false;
    }

    // Check if already loaded
    if (isAssetReady(handle.filename)) {
        return true;
    }

    try {
        // Read the render graph data
        auto [info, graphJson] = AssetLib::ReadRenderGraph(data);

        // Deserialize the template
        auto graphTemplate = RenderGraphSerialization::DeserializeGraphTemplate(graphJson);

        // Validate the template
        if (!graphTemplate->validate()) {
            throw std::runtime_error("RenderGraphTemplateManager: Invalid template loaded from " + handle.filename);
        }

        // Verify counts match
        if (graphTemplate->getNodeCount() != info.nodeCount ||
            graphTemplate->getConnectionCount() != info.connectionCount) {
            throw std::runtime_error("RenderGraphTemplateManager: Template node/connection count mismatch");
        }

        // Register the template
        registerTemplate(std::move(graphTemplate), handle.filename, false);

        return true;
    }
    catch (const std::exception& e) {
        // Log error (in real implementation)
        return false;
    }
}

void RenderGraphTemplateManager::unloadAsset(const std::string& filename) {
    auto it = m_templatesByFilename.find(filename);
    if (it != m_templatesByFilename.end()) {
        // Don't unload built-in templates
        if (it->second.isBuiltIn) {
            return;
        }

        // Remove from lookup maps
        m_templatesByName.erase(it->second.name);
        m_templatesByHandle.erase(it->second.handle);

        // Remove from main storage
        m_templatesByFilename.erase(it);
    }
}

bool RenderGraphTemplateManager::isAssetReady(const std::string& filename) const {
    return m_templatesByFilename.find(filename) != m_templatesByFilename.end();
}

uint64_t RenderGraphTemplateManager::getAssetSize(const std::string& filename) const {
    auto it = m_templatesByFilename.find(filename);
    if (it != m_templatesByFilename.end()) {
        return it->second.memorySize;
    }
    return 0;
}

std::vector<AssetDependency> RenderGraphTemplateManager::getDependencies(
    const AssetHandle& handle,
    const AssetLib::AssetData& data) const {
    // RenderGraphTemplates are pure structural data and don't have dependencies
    // on other assets at load time. Dependencies (like materials, meshes, etc.)
    // are determined at runtime when the graph is instantiated.
    return {};
}

std::any RenderGraphTemplateManager::getResourceInternal(const AssetHandle& handle) const {
    auto it = m_templatesByFilename.find(handle.filename);
    if (it != m_templatesByFilename.end()) {
        return std::any(it->second.template_.get());
    }
    return std::any(static_cast<RenderGraphTemplate*>(nullptr));
}

std::any RenderGraphTemplateManager::getHandleInternal(const std::string& filename) const {
    auto it = m_templatesByFilename.find(filename);
    if (it != m_templatesByFilename.end()) {
        return std::any(it->second.handle);
    }
    return std::any(RenderGraphTemplateHandle{});
}

RenderGraphTemplate* RenderGraphTemplateManager::getResource(RenderGraphTemplateHandle handle) const {
    const CachedTemplate* cached = getCachedTemplate(handle);
    return cached ? cached->template_.get() : nullptr;
}

bool RenderGraphTemplateManager::isAssetReady(RenderGraphTemplateHandle handle) const {
    return getCachedTemplate(handle) != nullptr;
}

const RenderGraphTemplate* RenderGraphTemplateManager::getTemplateByName(const std::string& name) const {
    auto it = m_templatesByName.find(name);
    if (it != m_templatesByName.end()) {
        return getResource(it->second);
    }
    return nullptr;
}

RenderGraphTemplateHandle RenderGraphTemplateManager::getHandleByName(const std::string& name) const {
    auto it = m_templatesByName.find(name);
    if (it != m_templatesByName.end()) {
        return it->second;
    }
    return RenderGraphTemplateHandle{};
}

SmartAssetHandle<RenderGraphTemplateHandle, RenderGraphTemplate>
RenderGraphTemplateManager::getTemplateSmartHandle(const std::string& name) const {
    RenderGraphTemplateHandle handle = getHandleByName(name);
    return createSmartHandle(handle);
}

std::unique_ptr<RenderGraphTemplate> RenderGraphTemplateManager::cloneTemplate(const std::string& name) const {
    const RenderGraphTemplate* original = getTemplateByName(name);
    if (!original) {
        return nullptr;
    }

    // Serialize and deserialize to create a deep copy
    nlohmann::json serialized = RenderGraphSerialization::SerializeGraphTemplate(*original);
    return RenderGraphSerialization::DeserializeGraphTemplate(serialized);
}

std::unique_ptr<RenderGraphTemplate> RenderGraphTemplateManager::cloneTemplate(RenderGraphTemplateHandle handle) const {
    const RenderGraphTemplate* original = getResource(handle);
    if (!original) {
        return nullptr;
    }

    // Serialize and deserialize to create a deep copy
    nlohmann::json serialized = RenderGraphSerialization::SerializeGraphTemplate(*original);
    return RenderGraphSerialization::DeserializeGraphTemplate(serialized);
}

bool RenderGraphTemplateManager::hasTemplate(const std::string& name) const {
    return m_templatesByName.find(name) != m_templatesByName.end();
}

const RenderGraphTemplateManager::CachedTemplate*
RenderGraphTemplateManager::getCachedTemplate(RenderGraphTemplateHandle handle) const {
    auto it = m_templatesByHandle.find(handle);
    return (it != m_templatesByHandle.end()) ? it->second : nullptr;
}

uint64_t RenderGraphTemplateManager::estimateTemplateSize(const RenderGraphTemplate& tmpl) const {
    // Rough estimation of memory usage
    uint64_t size = 0;

    // Base object size
    size += sizeof(RenderGraphTemplate);

    // Name string
    size += tmpl.getName().capacity();

    // Node templates
    size += tmpl.getNodeCount() * (sizeof(RenderNodeTemplate) + 100); // +100 for strings/vectors

    // Connections
    size += tmpl.getConnectionCount() * sizeof(NodeConnection);

    return size;
}