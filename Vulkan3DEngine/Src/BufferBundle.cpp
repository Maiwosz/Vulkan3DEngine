#include "BufferBundle.h"
#include <spdlog/spdlog.h>
#include <algorithm>

// =============================================================================
// BUFFER REGISTRATION
// =============================================================================

void BufferBundle::AddBuffer(
    const std::string& identifier,
    std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
    uint32_t binding
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!buffer) {
        SPDLOG_WARN("BufferBundle: Attempting to add null buffer '{}'", identifier);
        return;
    }

    BufferEntry entry;
    entry.identifier = identifier;
    entry.buffer = buffer;
    entry.binding = binding;

    m_buffers[identifier] = std::move(entry);

    // Rebuild routing cache
    BuildFieldRoutingCache();

    SPDLOG_DEBUG("BufferBundle: Added buffer '{}' with binding {}", identifier, binding);
}

void BufferBundle::RemoveBuffer(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end()) {
        m_buffers.erase(it);

        // Rebuild routing cache
        BuildFieldRoutingCache();

        SPDLOG_DEBUG("BufferBundle: Removed buffer '{}'", identifier);
    }
}

void BufferBundle::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_buffers.clear();
    m_fieldRoutingCache.clear();

    SPDLOG_DEBUG("BufferBundle: Cleared all buffers");
}

// =============================================================================
// BUFFER ACCESS
// =============================================================================

bool BufferBundle::HasBuffer(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_buffers.find(identifier) != m_buffers.end();
}

BufferBundle::BufferProxy BufferBundle::GetBuffer(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end()) {
        return BufferProxy(it->second.buffer);
    }

    return BufferProxy(nullptr);
}

BufferBundle::BufferProxy BufferBundle::operator[](const std::string& identifier) {
    return GetBuffer(identifier);
}

std::vector<std::string> BufferBundle::GetBufferIdentifiers() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> identifiers;
    identifiers.reserve(m_buffers.size());

    for (const auto& [id, _] : m_buffers) {
        identifiers.push_back(id);
    }

    std::sort(identifiers.begin(), identifiers.end());
    return identifiers;
}

// =============================================================================
// FIELD ROUTING
// =============================================================================

BufferBundle::BufferProxy BufferBundle::GetBufferForField(const std::string& path) {
    const FieldRouting* routing = FindRouting(path);
    if (!routing) {
        return BufferProxy(nullptr);
    }

    return GetBuffer(routing->bufferIdentifier);
}

std::string BufferBundle::GetBufferIdentifierForField(const std::string& path) const {
    const FieldRouting* routing = FindRouting(path);
    return routing ? routing->bufferIdentifier : "";
}

bool BufferBundle::HasField(const std::string& path) const {
    return FindRouting(path) != nullptr;
}

const BufferBundle::FieldInfo* BufferBundle::GetFieldInfo(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto routingIt = m_fieldRoutingCache.find(path);
    if (routingIt == m_fieldRoutingCache.end()) {
        return nullptr;
    }

    auto bufferIt = m_buffers.find(routingIt->second.bufferIdentifier);
    if (bufferIt == m_buffers.end() || !bufferIt->second.buffer) {
        return nullptr;
    }

    const ShaderLib::FieldDescriptor* descriptor =
        bufferIt->second.buffer->GetLayout()->FindField(path);

    if (!descriptor) {
        return nullptr;
    }

    // Create temporary FieldInfo (could be cached if needed)
    static thread_local FieldInfo tempInfo;
    tempInfo.path = path;
    tempInfo.bufferIdentifier = routingIt->second.bufferIdentifier;
    tempInfo.binding = routingIt->second.binding;
    tempInfo.descriptor = descriptor;

    return &tempInfo;
}

// =============================================================================
// CONVENIENT FIELD ACCESS
// =============================================================================

ShaderLib::FieldProxy BufferBundle::GetField(const std::string& path) {
    auto proxy = GetBufferForField(path);
    if (!proxy) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    return proxy->GetField(path);
}

// =============================================================================
// FIELD METADATA API - Delegated to BufferLayout
// =============================================================================

std::vector<std::string> BufferBundle::GetTopLevelFieldNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> allNames;

    for (const auto& [identifier, entry] : m_buffers) {
        if (!entry.buffer) continue;

        auto names = entry.buffer->GetLayout()->GetTopLevelFieldNames();
        allNames.insert(allNames.end(), names.begin(), names.end());
    }

    // Remove duplicates
    std::sort(allNames.begin(), allNames.end());
    allNames.erase(std::unique(allNames.begin(), allNames.end()), allNames.end());

    return allNames;
}

std::vector<std::string> BufferBundle::GetAllFieldPaths() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> allPaths;

    for (const auto& [identifier, entry] : m_buffers) {
        if (!entry.buffer) continue;

        auto paths = entry.buffer->GetLayout()->GetAllFieldPaths();
        allPaths.insert(allPaths.end(), paths.begin(), paths.end());
    }

    // Remove duplicates
    std::sort(allPaths.begin(), allPaths.end());
    allPaths.erase(std::unique(allPaths.begin(), allPaths.end()), allPaths.end());

    return allPaths;
}

std::vector<std::string> BufferBundle::GetFieldNames(const std::string& bufferIdentifier) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(bufferIdentifier);
    if (it == m_buffers.end() || !it->second.buffer) {
        return {};
    }

    return it->second.buffer->GetLayout()->GetTopLevelFieldNames();
}

bool BufferBundle::IsArrayField(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check each buffer's layout
    for (const auto& [identifier, entry] : m_buffers) {
        if (!entry.buffer) continue;

        if (entry.buffer->GetLayout()->IsArrayField(path)) {
            return true;
        }
    }

    return false;
}

bool BufferBundle::IsStructureField(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check each buffer's layout
    for (const auto& [identifier, entry] : m_buffers) {
        if (!entry.buffer) continue;

        if (entry.buffer->GetLayout()->IsStructureField(path)) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> BufferBundle::GetStructureChildren(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to find in each buffer
    for (const auto& [identifier, entry] : m_buffers) {
        if (!entry.buffer) continue;

        auto children = entry.buffer->GetLayout()->GetStructureChildren(path);
        if (!children.empty()) {
            return children;
        }
    }

    return {};
}

// =============================================================================
// CACHE BUILDING
// =============================================================================

void BufferBundle::BuildFieldRoutingCache() {
    // Caller must hold m_mutex
    m_fieldRoutingCache.clear();

    for (const auto& [bufferId, entry] : m_buffers) {
        if (!entry.buffer) {
            continue;
        }

        const auto& allFields = entry.buffer->GetLayout()->GetAllFields();

        for (const auto& field : allFields) {
            FieldRouting routing;
            routing.bufferIdentifier = bufferId;
            routing.binding = entry.binding;

            m_fieldRoutingCache[field.path] = routing;
        }
    }

    SPDLOG_DEBUG("BufferBundle: Built field routing cache with {} entries",
        m_fieldRoutingCache.size());
}

// =============================================================================
// HELPERS
// =============================================================================

const BufferBundle::FieldRouting* BufferBundle::FindRouting(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_fieldRoutingCache.find(path);
    if (it != m_fieldRoutingCache.end()) {
        return &it->second;
    }

    return nullptr;
}
