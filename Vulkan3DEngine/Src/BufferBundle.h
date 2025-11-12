#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "BufferObjectInstance.h"
#include "ShaderLib.h"

/**
 * BufferBundle - Field routing and buffer management
 *
 * Purpose:
 * - Route field paths to appropriate BufferObjectInstance
 * - Provide convenient access to buffers by identifier
 * - Delegate metadata queries to BufferLayout
 *
 * Design:
 * - Pure routing layer - NO synchronization logic
 * - Delegates all operations to BufferObjectInstance
 * - Delegates metadata queries to BufferLayout
 * - Thread-safe buffer registration/lookup
 */
class BufferBundle {
public:
    // ========================================================================
    // BUFFER ENTRY
    // ========================================================================

    struct BufferEntry {
        std::string identifier;
        std::shared_ptr<ShaderLib::BufferObjectInstance> buffer;
        uint32_t binding;
    };

    // ========================================================================
    // FIELD METADATA (simplified - just routing info)
    // ========================================================================

    struct FieldInfo {
        std::string path;
        std::string bufferIdentifier;
        uint32_t binding;
        const ShaderLib::FieldDescriptor* descriptor; // Points to actual descriptor in layout
    };

    // ========================================================================
    // BUFFER PROXY - Direct access to BufferObjectInstance
    // ========================================================================

    class BufferProxy {
    public:
        BufferProxy(std::shared_ptr<ShaderLib::BufferObjectInstance> buffer)
            : m_buffer(buffer) {
        }

        ShaderLib::BufferObjectInstance* operator->() { return m_buffer.get(); }
        const ShaderLib::BufferObjectInstance* operator->() const { return m_buffer.get(); }

        ShaderLib::BufferObjectInstance& operator*() { return *m_buffer; }
        const ShaderLib::BufferObjectInstance& operator*() const { return *m_buffer; }

        std::shared_ptr<ShaderLib::BufferObjectInstance> Get() { return m_buffer; }
        std::shared_ptr<const ShaderLib::BufferObjectInstance> Get() const { return m_buffer; }

        explicit operator bool() const { return m_buffer != nullptr; }

    private:
        std::shared_ptr<ShaderLib::BufferObjectInstance> m_buffer;
    };

    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    BufferBundle() = default;
    ~BufferBundle() = default;

    // ========================================================================
    // BUFFER REGISTRATION
    // ========================================================================

    void AddBuffer(
        const std::string& identifier,
        std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
        uint32_t binding
    );

    void RemoveBuffer(const std::string& identifier);
    void Clear();

    // ========================================================================
    // BUFFER ACCESS
    // ========================================================================

    bool HasBuffer(const std::string& identifier) const;
    BufferProxy GetBuffer(const std::string& identifier);
    BufferProxy operator[](const std::string& identifier);

    std::vector<std::string> GetBufferIdentifiers() const;
    size_t GetBufferCount() const { return m_buffers.size(); }

    // ========================================================================
    // FIELD ROUTING
    // ========================================================================

    BufferProxy GetBufferForField(const std::string& path);
    std::string GetBufferIdentifierForField(const std::string& path) const;
    bool HasField(const std::string& path) const;

    // Get field info with routing + descriptor
    const FieldInfo* GetFieldInfo(const std::string& path) const;

    // ========================================================================
    // CONVENIENT FIELD ACCESS (delegates to routed buffer)
    // ========================================================================

    template<typename T>
    T Get(const std::string& path) const;

    template<typename T>
    void Set(const std::string& path, const T& value);

    ShaderLib::FieldProxy GetField(const std::string& path);

    // ========================================================================
    // FIELD METADATA API - Delegated to BufferLayout
    // ========================================================================

    // Get all top-level field names from all buffers
    std::vector<std::string> GetTopLevelFieldNames() const;

    // Get all field paths from all buffers
    std::vector<std::string> GetAllFieldPaths() const;

    // Get field names for a specific buffer
    std::vector<std::string> GetFieldNames(const std::string& bufferIdentifier) const;

    // Query field properties (checks all buffers)
    bool IsArrayField(const std::string& path) const;
    bool IsStructureField(const std::string& path) const;
    std::vector<std::string> GetStructureChildren(const std::string& path) const;

private:
    // ========================================================================
    // INTERNAL STRUCTURES
    // ========================================================================

    struct FieldRouting {
        std::string bufferIdentifier;
        uint32_t binding;
    };

    // ========================================================================
    // CACHE BUILDING
    // ========================================================================

    void BuildFieldRoutingCache();

    // ========================================================================
    // HELPERS
    // ========================================================================

    const FieldRouting* FindRouting(const std::string& path) const;

    // ========================================================================
    // DATA
    // ========================================================================

    std::unordered_map<std::string, BufferEntry> m_buffers;
    std::unordered_map<std::string, FieldRouting> m_fieldRoutingCache;

    mutable std::mutex m_mutex;
};

// =============================================================================
// TEMPLATE IMPLEMENTATIONS
// =============================================================================

template<typename T>
inline T BufferBundle::Get(const std::string& path) const {
    auto proxy = const_cast<BufferBundle*>(this)->GetBufferForField(path);
    if (!proxy) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    return proxy->Get<T>(path);
}

template<typename T>
inline void BufferBundle::Set(const std::string& path, const T& value) {
    auto proxy = GetBufferForField(path);
    if (!proxy) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    proxy->Set(path, value);
}
