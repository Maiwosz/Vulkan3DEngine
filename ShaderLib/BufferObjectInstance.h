#pragma once
#include "BufferObjectDefinition.h"
#include "IBufferMapping.h"
#include "IAsyncMemoryOperations.h"
#include "FieldProxy.h"
#include <memory>
#include <vector>

namespace ShaderLib {

    /**
     * BufferObjectInstance
     *
     * Combines:
     * - BufferLayout (as map to byte buffer)
     * - Byte buffer (CPU-side data)
     * - GPU buffer mapping (optional)
     * - Async operations (optional)
     *
     * When IAsyncMemoryOperations is set, all sync operations automatically
     * use multi-threaded chunking. Additional async methods allow non-blocking
     * GPU synchronization and memory operations.
     */
    class BufferObjectInstance {
    public:
        explicit BufferObjectInstance(
            std::shared_ptr<const BufferObjectDefinition> definition
        );

        ~BufferObjectInstance() = default;

        // Disable copy, enable move
        BufferObjectInstance(const BufferObjectInstance&) = delete;
        BufferObjectInstance& operator=(const BufferObjectInstance&) = delete;
        BufferObjectInstance(BufferObjectInstance&&) = default;
        BufferObjectInstance& operator=(BufferObjectInstance&&) = default;

        // ========================================================================
        // DEFINITION & LAYOUT ACCESS
        // ========================================================================

        std::shared_ptr<const BufferObjectDefinition> GetDefinition() const {
            return m_definition;
        }

        std::shared_ptr<const BufferLayout> GetLayout() const {
            return m_definition->GetLayout();
        }

        // ========================================================================
        // RAW BUFFER ACCESS (CPU-side)
        // ========================================================================

        uint8_t* GetRawBuffer() { return m_buffer.data(); }
        const uint8_t* GetRawBuffer() const { return m_buffer.data(); }
        size_t GetBufferSize() const { return m_buffer.size(); }

        // ========================================================================
        // TYPED FIELD ACCESS - O(1) through path lookup
        // ========================================================================

        template<typename T>
        T Get(const std::string& path) const;

        template<typename T>
        void Set(const std::string& path, const T& value);

        // Bezpośredni dostęp do elementu tablicy (bardziej wydajne niż parsowanie path)
        template<typename T>
        T GetArrayElement(const std::string& arrayPath, uint32_t index) const;

        template<typename T>
        void SetArrayElement(const std::string& arrayPath, uint32_t index, const T& value);

        template<typename T>
        T GetAtOffset(uint32_t offset) const;

        template<typename T>
        void SetAtOffset(uint32_t offset, const T& value);

        // ========================================================================
        // PROXY ACCESS
        // ========================================================================

        FieldProxy operator[](const std::string& name);
        FieldProxy operator[](const char* name);
        FieldProxy GetField(const std::string& path);

        // ========================================================================
        // BULK OPERATIONS (blocking)
        // Uses async operations if available for automatic multi-threading
        // ========================================================================

        void CopyFrom(const BufferObjectInstance& other);
        void CopyRegion(
            const BufferObjectInstance& other,
            uint32_t srcOffset,
            uint32_t dstOffset,
            uint32_t size
        );

        void Zero();
        void ZeroRegion(uint32_t offset, uint32_t size);

        // ========================================================================
        // BULK OPERATIONS (non-blocking)
        // Only available when async operations interface is set
        // ========================================================================

        AsyncOperationHandle CopyFromAsync(const BufferObjectInstance& other);
        AsyncOperationHandle CopyRegionAsync(
            const BufferObjectInstance& other,
            uint32_t srcOffset,
            uint32_t dstOffset,
            uint32_t size
        );

        AsyncOperationHandle ZeroAsync();
        AsyncOperationHandle ZeroRegionAsync(uint32_t offset, uint32_t size);

        // ========================================================================
        // GPU BUFFER MAPPING
        // ========================================================================

        void SetMappedBuffer(IBufferMapping* buffer);
        IBufferMapping* GetMappedBuffer() const { return m_mappedBuffer; }
        bool HasMappedBuffer() const { return m_mappedBuffer != nullptr; }
        bool IsBufferMapped() const;

        void* GetMappedPointer();
        const void* GetMappedPointer() const;

        // ========================================================================
        // GPU SYNCHRONIZATION (blocking)
        // Uses async operations if available for automatic multi-threading
        // ========================================================================

        void SyncToBuffer();
        void SyncFromBuffer();
        void SyncRangeToBuffer(uint32_t offset, uint32_t size);
        void SyncRangeFromBuffer(uint32_t offset, uint32_t size);

        // ========================================================================
        // ASYNC GPU SYNCHRONIZATION (non-blocking)
        // Only available when async operations interface is set
        // ========================================================================

        AsyncOperationHandle SyncToBufferAsync();
        AsyncOperationHandle SyncFromBufferAsync();
        AsyncOperationHandle SyncRangeToBufferAsync(uint32_t offset, uint32_t size);
        AsyncOperationHandle SyncRangeFromBufferAsync(uint32_t offset, uint32_t size);

        // ========================================================================
        // DIRECT GPU ACCESS (blocking)
        // Uses async operations if available for automatic multi-threading
        // ========================================================================

        template<typename T>
        T GetFromGPU(const std::string& path) const;

        template<typename T>
        void SetToGPU(const std::string& path, const T& value);

        template<typename T>
        T GetFromGPUAtOffset(uint32_t offset) const;

        template<typename T>
        void SetToGPUAtOffset(uint32_t offset, const T& value);

        void CopyToGPUDirect(const void* source, uint32_t offset, uint32_t size);
        void CopyFromGPUDirect(void* destination, uint32_t offset, uint32_t size);

        // ========================================================================
        // DIRECT GPU ACCESS (non-blocking)
        // Only available when async operations interface is set
        // ========================================================================

        AsyncOperationHandle CopyToGPUDirectAsync(
            const void* source,
            uint32_t offset,
            uint32_t size
        );

        AsyncOperationHandle CopyFromGPUDirectAsync(
            void* destination,
            uint32_t offset,
            uint32_t size
        );

        // ========================================================================
        // ASYNC OPERATION MANAGEMENT
        // ========================================================================

        bool IsSyncComplete(AsyncOperationHandle handle);
        void WaitForSync(AsyncOperationHandle handle);
        void WaitForAllSyncs();

        // ========================================================================
        // ASYNC OPERATIONS INTERFACE
        // ========================================================================

        void SetAsyncOperations(IAsyncMemoryOperations* asyncOps);
        IAsyncMemoryOperations* GetAsyncOperations() const { return m_asyncOps; }
        bool HasAsyncOperations() const { return m_asyncOps != nullptr; }

        // ========================================================================
        // CPU BUFFER STATE MANAGEMENT
        // ========================================================================

        bool IsCPUBufferValid() const { return m_cpuBufferValid; }
        void MarkCPUBufferInvalid() { m_cpuBufferValid = false; }
        void MarkCPUBufferValid() { m_cpuBufferValid = true; }

        // ========================================================================
        // CLONING & SERIALIZATION
        // ========================================================================

        std::shared_ptr<BufferObjectInstance> Clone() const;

        json ToJson() const;
        bool FromJson(const json& j);

        static std::shared_ptr<BufferObjectInstance> FromJson(
            const json& j,
            std::shared_ptr<const BufferObjectDefinition> definition
        );

    private:
        void InitializeBuffer();
        void ValidateOffset(uint32_t offset, uint32_t size) const;
        void ValidateBufferSize() const;
        void ValidateSyncRange(uint32_t offset, uint32_t size) const;
        void ValidateGPUAccess() const;
        void ValidateAsyncAvailable() const;

        // Sync helpers (internal)
        void SyncToBufferInternal(uint32_t offset, uint32_t size);
        void SyncFromBufferInternal(uint32_t offset, uint32_t size);

        std::shared_ptr<const BufferObjectDefinition> m_definition;
        std::vector<uint8_t> m_buffer;  // CPU-side data
        IBufferMapping* m_mappedBuffer;  // GPU mapping (non-owning)
        IAsyncMemoryOperations* m_asyncOps;  // Async operations (non-owning)
        bool m_cpuBufferValid = true;

        friend class FieldProxy;
    };

    // ============================================================================
    // TEMPLATE IMPLEMENTATIONS - CPU ACCESS
    // ============================================================================

    template<typename T>
    inline T BufferObjectInstance::Get(const std::string& path) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        // Parsuj ścieżkę - może zawierać indeks tablicy
        std::string cleanPath = path;
        uint32_t arrayIndex = 0;
        bool hasIndex = false;

        size_t bracketPos = path.find('[');
        if (bracketPos != std::string::npos) {
            cleanPath = path.substr(0, bracketPos);

            size_t endBracket = path.find(']', bracketPos);
            if (endBracket != std::string::npos) {
                std::string indexStr = path.substr(bracketPos + 1, endBracket - bracketPos - 1);
                arrayIndex = static_cast<uint32_t>(std::stoul(indexStr));
                hasIndex = true;
            }
        }

        const FieldDescriptor* field = m_definition->FindField(cleanPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot get structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        // Oblicz offset
        uint32_t offset;
        if (field->isArray) {
            if (!hasIndex) {
                throw std::runtime_error("Array field requires index: " + path);
            }
            offset = field->GetElementOffset(arrayIndex);
        }
        else {
            if (hasIndex) {
                throw std::runtime_error("Field is not an array: " + path);
            }
            offset = field->offset;
        }

        return GetAtOffset<T>(offset);
    }

    template<typename T>
    inline void BufferObjectInstance::Set(const std::string& path, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        // Parsuj ścieżkę - może zawierać indeks tablicy
        std::string cleanPath = path;
        uint32_t arrayIndex = 0;
        bool hasIndex = false;

        size_t bracketPos = path.find('[');
        if (bracketPos != std::string::npos) {
            cleanPath = path.substr(0, bracketPos);

            size_t endBracket = path.find(']', bracketPos);
            if (endBracket != std::string::npos) {
                std::string indexStr = path.substr(bracketPos + 1, endBracket - bracketPos - 1);
                arrayIndex = static_cast<uint32_t>(std::stoul(indexStr));
                hasIndex = true;
            }
        }

        const FieldDescriptor* field = m_definition->FindField(cleanPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot set structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        // Oblicz offset
        uint32_t offset;
        if (field->isArray) {
            if (!hasIndex) {
                throw std::runtime_error("Array field requires index: " + path);
            }
            offset = field->GetElementOffset(arrayIndex);
        }
        else {
            if (hasIndex) {
                throw std::runtime_error("Field is not an array: " + path);
            }
            offset = field->offset;
        }

        SetAtOffset(offset, value);
    }

    // Wydajniejsze metody dla bezpośredniego dostępu do tablic
    template<typename T>
    inline T BufferObjectInstance::GetArrayElement(const std::string& arrayPath, uint32_t index) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        const FieldDescriptor* field = m_definition->FindField(arrayPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + arrayPath);
        }

        if (!field->isArray) {
            throw std::runtime_error("Field is not an array: " + arrayPath);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot get structure array element as value: " + arrayPath);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + arrayPath);
        }

        uint32_t offset = field->GetElementOffset(index);
        return GetAtOffset<T>(offset);
    }

    template<typename T>
    inline void BufferObjectInstance::SetArrayElement(const std::string& arrayPath, uint32_t index, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        const FieldDescriptor* field = m_definition->FindField(arrayPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + arrayPath);
        }

        if (!field->isArray) {
            throw std::runtime_error("Field is not an array: " + arrayPath);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot set structure array element as value: " + arrayPath);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + arrayPath);
        }

        uint32_t offset = field->GetElementOffset(index);
        SetAtOffset(offset, value);
    }

    template<typename T>
    inline T BufferObjectInstance::GetAtOffset(uint32_t offset) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        ValidateOffset(offset, sizeof(T));

        T value;
        std::memcpy(&value, m_buffer.data() + offset, sizeof(T));
        return value;
    }

    template<typename T>
    inline void BufferObjectInstance::SetAtOffset(uint32_t offset, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        ValidateOffset(offset, sizeof(T));

        std::memcpy(m_buffer.data() + offset, &value, sizeof(T));
    }

    // ============================================================================
    // TEMPLATE IMPLEMENTATIONS - DIRECT GPU ACCESS
    // ============================================================================

    template<typename T>
    inline T BufferObjectInstance::GetFromGPU(const std::string& path) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        // Parsuj ścieżkę
        std::string cleanPath = path;
        uint32_t arrayIndex = 0;
        bool hasIndex = false;

        size_t bracketPos = path.find('[');
        if (bracketPos != std::string::npos) {
            cleanPath = path.substr(0, bracketPos);

            size_t endBracket = path.find(']', bracketPos);
            if (endBracket != std::string::npos) {
                std::string indexStr = path.substr(bracketPos + 1, endBracket - bracketPos - 1);
                arrayIndex = static_cast<uint32_t>(std::stoul(indexStr));
                hasIndex = true;
            }
        }

        const FieldDescriptor* field = m_definition->FindField(cleanPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot get structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        uint32_t offset;
        if (field->isArray) {
            if (!hasIndex) {
                throw std::runtime_error("Array field requires index: " + path);
            }
            offset = field->GetElementOffset(arrayIndex);
        }
        else {
            if (hasIndex) {
                throw std::runtime_error("Field is not an array: " + path);
            }
            offset = field->offset;
        }

        return GetFromGPUAtOffset<T>(offset);
    }

    template<typename T>
    inline void BufferObjectInstance::SetToGPU(const std::string& path, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        // Parsuj ścieżkę
        std::string cleanPath = path;
        uint32_t arrayIndex = 0;
        bool hasIndex = false;

        size_t bracketPos = path.find('[');
        if (bracketPos != std::string::npos) {
            cleanPath = path.substr(0, bracketPos);

            size_t endBracket = path.find(']', bracketPos);
            if (endBracket != std::string::npos) {
                std::string indexStr = path.substr(bracketPos + 1, endBracket - bracketPos - 1);
                arrayIndex = static_cast<uint32_t>(std::stoul(indexStr));
                hasIndex = true;
            }
        }

        const FieldDescriptor* field = m_definition->FindField(cleanPath);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot set structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        uint32_t offset;
        if (field->isArray) {
            if (!hasIndex) {
                throw std::runtime_error("Array field requires index: " + path);
            }
            offset = field->GetElementOffset(arrayIndex);
        }
        else {
            if (hasIndex) {
                throw std::runtime_error("Field is not an array: " + path);
            }
            offset = field->offset;
        }

        SetToGPUAtOffset(offset, value);
    }

    template<typename T>
    inline T BufferObjectInstance::GetFromGPUAtOffset(uint32_t offset) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        ValidateOffset(offset, sizeof(T));
        ValidateGPUAccess();

        const void* gpuData = GetMappedPointer();

        T value;
        std::memcpy(&value, static_cast<const uint8_t*>(gpuData) + offset, sizeof(T));
        return value;
    }

    template<typename T>
    inline void BufferObjectInstance::SetToGPUAtOffset(uint32_t offset, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        ValidateOffset(offset, sizeof(T));
        ValidateGPUAccess();

        void* gpuData = GetMappedPointer();
        std::memcpy(static_cast<uint8_t*>(gpuData) + offset, &value, sizeof(T));

        m_cpuBufferValid = false;
    }

} // namespace ShaderLib
