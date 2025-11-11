#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "BufferObjectInstance.h"
#include "AsyncBufferSync.h"
#include "ThreadPool.h"
#include "ShaderLib.h"

class BufferBundle {
public:
    struct BufferEntry {
        std::string identifier;
        std::shared_ptr<ShaderLib::BufferObjectInstance> buffer;
        std::unique_ptr<AsyncBufferSync> asyncSync;
        uint32_t binding;
    };

    // Field metadata
    struct FieldInfo {
        std::string name;
        std::string path;
        std::string bufferIdentifier;  // Który bufor zawiera to pole
        ShaderLib::BaseType baseType;
        uint32_t binding;
        bool isBaseType;
        bool isArray;
        uint32_t arraySize;
        uint32_t offset;
        uint32_t size;
    };

    explicit BufferBundle(ThreadPool& threadPool);
    ~BufferBundle();

    // Buffer registration
    void AddBuffer(
        const std::string& identifier,
        std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
        uint32_t binding
    );

    void RemoveBuffer(const std::string& identifier);
    void Clear();

    // Buffer queries
    bool HasBuffer(const std::string& identifier) const;
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetBuffer(const std::string& identifier) const;
    std::vector<std::string> GetBufferIdentifiers() const;
    size_t GetBufferCount() const { return m_buffers.size(); }

    // Field routing
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetBufferForField(const std::string& path) const;
    std::string GetBufferIdentifierForField(const std::string& path) const;

    // Field access
    ShaderLib::FieldProxy GetField(const std::string& path);
    bool HasField(const std::string& path) const;

    template<typename T>
    T Get(const std::string& path) const;

    template<typename T>
    void Set(const std::string& path, const T& value);

    // ========================================================================
    // FIELD METADATA API
    // ========================================================================

    // Pobierz informacje o polu (po nazwie lub ścieżce)
    const FieldInfo* GetFieldInfo(const std::string& nameOrPath) const;

    // Pobierz wszystkie top-level nazwy pól
    std::vector<std::string> GetTopLevelFieldNames() const;

    // Pobierz wszystkie ścieżki pól
    std::vector<std::string> GetAllFieldPaths() const;

    // Pobierz ścieżki pól należące do danego top-level pola
    std::vector<std::string> GetFieldPaths(const std::string& topLevelName) const;

    // Sprawdź czy pole jest tablicą
    bool IsArrayField(const std::string& name) const;

    // Pobierz rozmiar tablicy
    size_t GetArraySize(const std::string& name) const;

    // Sprawdź czy pole jest strukturą
    bool IsStructureField(const std::string& name) const;

    // Pobierz dzieci struktury
    std::vector<std::string> GetStructureChildren(const std::string& name) const;

    // ========================================================================
    // SYNCHRONOUS OPERATIONS
    // ========================================================================

    void SyncAllToGPU();
    void SyncAllFromGPU();
    void SyncBufferToGPU(const std::string& identifier);
    void SyncBufferFromGPU(const std::string& identifier);
    void SyncFieldsToGPU(const std::vector<std::string>& paths);
    void SyncFieldsFromGPU(const std::vector<std::string>& paths);

    // ========================================================================
    // ASYNCHRONOUS OPERATIONS
    // ========================================================================

    std::vector<BufferSyncTaskHandle> SyncAllToGPUAsync();
    std::vector<BufferSyncTaskHandle> SyncAllFromGPUAsync();
    BufferSyncTaskHandle SyncBufferToGPUAsync(const std::string& identifier);
    BufferSyncTaskHandle SyncBufferFromGPUAsync(const std::string& identifier);
    std::vector<BufferSyncTaskHandle> SyncFieldsToGPUAsync(const std::vector<std::string>& paths);
    std::vector<BufferSyncTaskHandle> SyncFieldsFromGPUAsync(const std::vector<std::string>& paths);

    // ========================================================================
    // TASK MANAGEMENT
    // ========================================================================

    bool AreTasksComplete(const std::vector<BufferSyncTaskHandle>& tasks) const;
    void WaitForTasks(const std::vector<BufferSyncTaskHandle>& tasks);
    void WaitForAllTasks();
    void PollCompletedTasks();
    size_t GetActiveTaskCount() const;

private:
    struct FieldRouting {
        std::string bufferIdentifier;
        uint32_t binding;
    };

    // Cache building
    void BuildFieldRoutingCache();
    void BuildFieldMetadataCache();

    // Helpers
    const FieldRouting* FindRouting(const std::string& path) const;
    std::unordered_map<std::string, std::vector<std::string>> GroupFieldsByBuffer(
        const std::vector<std::string>& paths
    ) const;
    std::string ExtractTopLevelName(const std::string& path) const;

    // Field metadata helpers
    const FieldInfo* FindFieldInfo(const std::string& nameOrPath) const;

    ThreadPool& m_threadPool;
    std::unordered_map<std::string, BufferEntry> m_buffers;

    // Field routing cache
    std::unordered_map<std::string, FieldRouting> m_fieldRoutingCache;

    // Field metadata cache (przeniesione z Material)
    std::unordered_map<std::string, FieldInfo> m_fieldInfoCache;
    std::unordered_map<std::string, std::vector<std::string>> m_topLevelToPaths;

    mutable std::mutex m_mutex;
};

// =============================================================================
// TEMPLATE IMPLEMENTATIONS
// =============================================================================

template<typename T>
inline T BufferBundle::Get(const std::string& path) const {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    return buffer->Get<T>(path);
}

template<typename T>
inline void BufferBundle::Set(const std::string& path, const T& value) {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("BufferBundle: Field not found: " + path);
    }
    buffer->Set(path, value);
}
