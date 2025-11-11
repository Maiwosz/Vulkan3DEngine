#include "BufferBundle.h"
#include <spdlog/spdlog.h>
#include <algorithm>

BufferBundle::BufferBundle(ThreadPool& threadPool)
    : m_threadPool(threadPool)
{
}

BufferBundle::~BufferBundle() {
    WaitForAllTasks();
    Clear();
}

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
    entry.asyncSync = std::make_unique<AsyncBufferSync>(buffer, m_threadPool);

    m_buffers[identifier] = std::move(entry);

    // Rebuild routing cache
    BuildFieldRoutingCache();

    SPDLOG_DEBUG("BufferBundle: Added buffer '{}' with binding {}", identifier, binding);
}

void BufferBundle::RemoveBuffer(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end()) {
        // Wait for pending tasks on this buffer
        if (it->second.asyncSync) {
            it->second.asyncSync->WaitForAllTasks();
        }

        m_buffers.erase(it);
        BuildFieldRoutingCache();

        SPDLOG_DEBUG("BufferBundle: Removed buffer '{}'", identifier);
    }
}

void BufferBundle::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Wait for all tasks
    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            entry.asyncSync->WaitForAllTasks();
        }
    }

    m_buffers.clear();
    m_fieldRoutingCache.clear();

    SPDLOG_DEBUG("BufferBundle: Cleared all buffers");
}

// =============================================================================
// BUFFER QUERIES
// =============================================================================

bool BufferBundle::HasBuffer(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_buffers.find(identifier) != m_buffers.end();
}

std::shared_ptr<ShaderLib::BufferObjectInstance> BufferBundle::GetBuffer(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end()) {
        return it->second.buffer;
    }

    return nullptr;
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

std::shared_ptr<ShaderLib::BufferObjectInstance> BufferBundle::GetBufferForField(const std::string& path) const {
    const FieldRouting* routing = FindRouting(path);
    if (!routing) {
        return nullptr;
    }

    return GetBuffer(routing->bufferIdentifier);
}

std::string BufferBundle::GetBufferIdentifierForField(const std::string& path) const {
    const FieldRouting* routing = FindRouting(path);
    return routing ? routing->bufferIdentifier : "";
}

// =============================================================================
// FIELD ACCESS
// =============================================================================

ShaderLib::FieldProxy BufferBundle::GetField(const std::string& path) {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    return buffer->GetField(path);
}

bool BufferBundle::HasField(const std::string& path) const {
    return FindRouting(path) != nullptr;
}

// =============================================================================
// FIELD METADATA API
// =============================================================================

const BufferBundle::FieldInfo* BufferBundle::GetFieldInfo(const std::string& nameOrPath) const {
    return FindFieldInfo(nameOrPath);
}

std::vector<std::string> BufferBundle::GetTopLevelFieldNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    names.reserve(m_topLevelToPaths.size());

    for (const auto& [name, _] : m_topLevelToPaths) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> BufferBundle::GetAllFieldPaths() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> paths;
    paths.reserve(m_fieldInfoCache.size());

    for (const auto& [path, _] : m_fieldInfoCache) {
        paths.push_back(path);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

std::vector<std::string> BufferBundle::GetFieldPaths(const std::string& topLevelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_topLevelToPaths.find(topLevelName);
    if (it != m_topLevelToPaths.end()) {
        std::vector<std::string> paths = it->second;
        std::sort(paths.begin(), paths.end());
        return paths;
    }

    return {};
}

bool BufferBundle::IsArrayField(const std::string& name) const {
    const FieldInfo* info = FindFieldInfo(name);
    return info && info->isArray;
}

size_t BufferBundle::GetArraySize(const std::string& name) const {
    const FieldInfo* info = FindFieldInfo(name);
    return info ? info->arraySize : 0;
}

bool BufferBundle::IsStructureField(const std::string& name) const {
    const FieldInfo* info = FindFieldInfo(name);
    return info && !info->isBaseType;
}

std::vector<std::string> BufferBundle::GetStructureChildren(const std::string& name) const {
    if (!IsStructureField(name)) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> children;
    std::string prefix = name + ".";

    // Find all paths that start with "name."
    for (const auto& [path, _] : m_fieldInfoCache) {
        if (path.find(prefix) == 0) {
            // Extract immediate child name (without further nesting)
            std::string remainder = path.substr(prefix.length());
            size_t dotPos = remainder.find('.');
            size_t bracketPos = remainder.find('[');
            size_t endPos = std::min(dotPos, bracketPos);

            std::string childName = (endPos != std::string::npos)
                ? remainder.substr(0, endPos)
                : remainder;

            // Add unique child names
            if (std::find(children.begin(), children.end(), childName) == children.end()) {
                children.push_back(childName);
            }
        }
    }

    std::sort(children.begin(), children.end());
    return children;
}

// =============================================================================
// INTERNAL HELPERS - FIELD METADATA
// =============================================================================

void BufferBundle::BuildFieldMetadataCache() {
    // Caller must hold m_mutex
    m_fieldInfoCache.clear();
    m_topLevelToPaths.clear();

    for (const auto& [bufferId, entry] : m_buffers) {
        if (!entry.buffer) {
            continue;
        }

        const auto& allFields = entry.buffer->GetDefinition()->GetAllFields();

        for (const auto& field : allFields) {
            FieldInfo info;
            info.name = field.name;
            info.path = field.path;
            info.bufferIdentifier = bufferId;
            info.baseType = field.baseType;
            info.binding = entry.binding;
            info.isBaseType = field.isBaseType;
            info.isArray = field.isArray;
            info.arraySize = field.arraySize;
            info.offset = field.offset;
            info.size = field.size;

            m_fieldInfoCache[field.path] = info;

            std::string topLevelName = ExtractTopLevelName(field.path);
            m_topLevelToPaths[topLevelName].push_back(field.path);
        }
    }

    SPDLOG_DEBUG("BufferBundle: Built metadata cache with {} fields ({} top-level)",
        m_fieldInfoCache.size(), m_topLevelToPaths.size());
}

const BufferBundle::FieldInfo* BufferBundle::FindFieldInfo(const std::string& nameOrPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try as path first (full match)
    auto it = m_fieldInfoCache.find(nameOrPath);
    if (it != m_fieldInfoCache.end()) {
        return &it->second;
    }

    // Try as top-level name
    auto topIt = m_topLevelToPaths.find(nameOrPath);
    if (topIt != m_topLevelToPaths.end() && !topIt->second.empty()) {
        const std::string& firstPath = topIt->second[0];
        auto infoIt = m_fieldInfoCache.find(firstPath);
        if (infoIt != m_fieldInfoCache.end()) {
            return &infoIt->second;
        }
    }

    return nullptr;
}

std::string BufferBundle::ExtractTopLevelName(const std::string& path) const {
    // Extract name before first '.' or '['
    size_t dotPos = path.find('.');
    size_t bracketPos = path.find('[');
    size_t endPos = std::min(dotPos, bracketPos);

    if (endPos == std::string::npos) {
        return path;
    }

    return path.substr(0, endPos);
}

// =============================================================================
// SYNCHRONOUS OPERATIONS
// =============================================================================

void BufferBundle::SyncAllToGPU() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            entry.asyncSync->SyncToGPU();
        }
    }

    SPDLOG_TRACE("BufferBundle: Synced all buffers to GPU");
}

void BufferBundle::SyncAllFromGPU() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            entry.asyncSync->SyncFromGPU();
        }
    }

    SPDLOG_TRACE("BufferBundle: Synced all buffers from GPU");
}

void BufferBundle::SyncBufferToGPU(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end() && it->second.asyncSync) {
        it->second.asyncSync->SyncToGPU();
        SPDLOG_TRACE("BufferBundle: Synced buffer '{}' to GPU", identifier);
    }
}

void BufferBundle::SyncBufferFromGPU(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end() && it->second.asyncSync) {
        it->second.asyncSync->SyncFromGPU();
        SPDLOG_TRACE("BufferBundle: Synced buffer '{}' from GPU", identifier);
    }
}

void BufferBundle::SyncFieldsToGPU(const std::vector<std::string>& paths) {
    auto groupedFields = GroupFieldsByBuffer(paths);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [bufferId, fieldPaths] : groupedFields) {
        auto it = m_buffers.find(bufferId);
        if (it != m_buffers.end() && it->second.asyncSync) {
            it->second.asyncSync->SyncFieldsToGPU(fieldPaths);
        }
    }

    SPDLOG_TRACE("BufferBundle: Synced {} fields to GPU", paths.size());
}

void BufferBundle::SyncFieldsFromGPU(const std::vector<std::string>& paths) {
    auto groupedFields = GroupFieldsByBuffer(paths);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [bufferId, fieldPaths] : groupedFields) {
        auto it = m_buffers.find(bufferId);
        if (it != m_buffers.end() && it->second.asyncSync) {
            it->second.asyncSync->SyncFieldsFromGPU(fieldPaths);
        }
    }

    SPDLOG_TRACE("BufferBundle: Synced {} fields from GPU", paths.size());
}

// =============================================================================
// ASYNCHRONOUS OPERATIONS
// =============================================================================

std::vector<BufferSyncTaskHandle> BufferBundle::SyncAllToGPUAsync() {
    std::vector<BufferSyncTaskHandle> tasks;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            BufferSyncTaskHandle task = entry.asyncSync->SyncToGPUAsync();
            if (task.isValid()) {
                tasks.push_back(task);
            }
        }
    }

    SPDLOG_TRACE("BufferBundle: Started async sync all to GPU ({} tasks)", tasks.size());
    return tasks;
}

std::vector<BufferSyncTaskHandle> BufferBundle::SyncAllFromGPUAsync() {
    std::vector<BufferSyncTaskHandle> tasks;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            BufferSyncTaskHandle task = entry.asyncSync->SyncFromGPUAsync();
            if (task.isValid()) {
                tasks.push_back(task);
            }
        }
    }

    SPDLOG_TRACE("BufferBundle: Started async sync all from GPU ({} tasks)", tasks.size());
    return tasks;
}

BufferSyncTaskHandle BufferBundle::SyncBufferToGPUAsync(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end() && it->second.asyncSync) {
        BufferSyncTaskHandle task = it->second.asyncSync->SyncToGPUAsync();
        SPDLOG_TRACE("BufferBundle: Started async sync buffer '{}' to GPU", identifier);
        return task;
    }

    return BufferSyncTaskHandle();
}

BufferSyncTaskHandle BufferBundle::SyncBufferFromGPUAsync(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_buffers.find(identifier);
    if (it != m_buffers.end() && it->second.asyncSync) {
        BufferSyncTaskHandle task = it->second.asyncSync->SyncFromGPUAsync();
        SPDLOG_TRACE("BufferBundle: Started async sync buffer '{}' from GPU", identifier);
        return task;
    }

    return BufferSyncTaskHandle();
}

std::vector<BufferSyncTaskHandle> BufferBundle::SyncFieldsToGPUAsync(const std::vector<std::string>& paths) {
    std::vector<BufferSyncTaskHandle> tasks;
    auto groupedFields = GroupFieldsByBuffer(paths);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [bufferId, fieldPaths] : groupedFields) {
        auto it = m_buffers.find(bufferId);
        if (it != m_buffers.end() && it->second.asyncSync) {
            BufferSyncTaskHandle task = it->second.asyncSync->SyncFieldsToGPUAsync(fieldPaths);
            if (task.isValid()) {
                tasks.push_back(task);
            }
        }
    }

    SPDLOG_TRACE("BufferBundle: Started async field sync to GPU ({} fields, {} tasks)",
        paths.size(), tasks.size());
    return tasks;
}

std::vector<BufferSyncTaskHandle> BufferBundle::SyncFieldsFromGPUAsync(const std::vector<std::string>& paths) {
    std::vector<BufferSyncTaskHandle> tasks;
    auto groupedFields = GroupFieldsByBuffer(paths);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [bufferId, fieldPaths] : groupedFields) {
        auto it = m_buffers.find(bufferId);
        if (it != m_buffers.end() && it->second.asyncSync) {
            BufferSyncTaskHandle task = it->second.asyncSync->SyncFieldsFromGPUAsync(fieldPaths);
            if (task.isValid()) {
                tasks.push_back(task);
            }
        }
    }

    SPDLOG_TRACE("BufferBundle: Started async field sync from GPU ({} fields, {} tasks)",
        paths.size(), tasks.size());
    return tasks;
}

// =============================================================================
// TASK MANAGEMENT
// =============================================================================

bool BufferBundle::AreTasksComplete(const std::vector<BufferSyncTaskHandle>& tasks) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& task : tasks) {
        // Find which buffer owns this task
        for (const auto& [id, entry] : m_buffers) {
            if (entry.asyncSync && !entry.asyncSync->IsTaskComplete(task)) {
                return false;
            }
        }
    }

    return true;
}

void BufferBundle::WaitForTasks(const std::vector<BufferSyncTaskHandle>& tasks) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& task : tasks) {
        for (auto& [id, entry] : m_buffers) {
            if (entry.asyncSync) {
                entry.asyncSync->WaitForTask(task);
            }
        }
    }

    SPDLOG_TRACE("BufferBundle: Waited for {} tasks", tasks.size());
}

void BufferBundle::WaitForAllTasks() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            entry.asyncSync->WaitForAllTasks();
        }
    }

    SPDLOG_TRACE("BufferBundle: Waited for all tasks");
}

void BufferBundle::PollCompletedTasks() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            entry.asyncSync->PollCompletedTasks();
        }
    }
}

size_t BufferBundle::GetActiveTaskCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t total = 0;
    for (const auto& [id, entry] : m_buffers) {
        if (entry.asyncSync) {
            total += entry.asyncSync->GetActiveTaskCount();
        }
    }

    return total;
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

void BufferBundle::BuildFieldRoutingCache() {
    // Caller must hold m_mutex
    m_fieldRoutingCache.clear();

    for (const auto& [bufferId, entry] : m_buffers) {
        if (!entry.buffer) {
            continue;
        }

        // Get all fields from this buffer
        const auto& allFields = entry.buffer->GetDefinition()->GetAllFields();

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

const BufferBundle::FieldRouting* BufferBundle::FindRouting(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_fieldRoutingCache.find(path);
    if (it != m_fieldRoutingCache.end()) {
        return &it->second;
    }

    return nullptr;
}

std::unordered_map<std::string, std::vector<std::string>> BufferBundle::GroupFieldsByBuffer(
    const std::vector<std::string>& paths
) const {
    std::unordered_map<std::string, std::vector<std::string>> grouped;

    for (const auto& path : paths) {
        const FieldRouting* routing = FindRouting(path);
        if (routing) {
            grouped[routing->bufferIdentifier].push_back(path);
        }
    }

    return grouped;
}
