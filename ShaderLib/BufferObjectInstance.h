#pragma once
#include "BufferObjectDefinition.h"
#include "IBufferMapping.h"
#include "FieldProxy.h"
#include <memory>
#include <vector>

namespace ShaderLib {

    // ============================================================================
    // BUFFER OBJECT INSTANCE
    // 
    // Combines:
    // - BufferLayout (as map to byte buffer)
    // - Byte buffer (CPU-side data)
    // - GPU buffer mapping (optional)
    // 
    // Replaces old StructureInstance + BufferObjectInstance split.
    // ============================================================================

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

        // Direct offset access
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
        // BULK OPERATIONS
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
        // GPU BUFFER MAPPING
        // ========================================================================

        void SetMappedBuffer(IBufferMapping* buffer);
        IBufferMapping* GetMappedBuffer() const { return m_mappedBuffer; }
        bool HasMappedBuffer() const { return m_mappedBuffer != nullptr; }
        bool IsBufferMapped() const;

        void* GetMappedPointer();
        const void* GetMappedPointer() const;

        // ========================================================================
        // GPU SYNCHRONIZATION
        // ========================================================================

        // Full buffer sync
        void SyncToBuffer();
        void SyncFromBuffer();

        // Range sync
        void SyncRangeToBuffer(uint32_t offset, uint32_t size);
        void SyncRangeFromBuffer(uint32_t offset, uint32_t size);

        // Single field sync (by path)
        void SyncFieldToBuffer(const std::string& path);
        void SyncFieldFromBuffer(const std::string& path);

        // Multi-field sync (by paths)
        void SyncFieldsToBuffer(const std::vector<std::string>& paths);
        void SyncFieldsFromBuffer(const std::vector<std::string>& paths);

        // Index-based sync
        void SyncFieldToBufferByIndex(size_t fieldIndex);
        void SyncFieldFromBufferByIndex(size_t fieldIndex);

        void SyncFieldsToBufferByIndices(const std::vector<size_t>& fieldIndices);
        void SyncFieldsFromBufferByIndices(const std::vector<size_t>& fieldIndices);

        void SyncFieldRangeToBuffer(size_t startIndex, size_t endIndex);
        void SyncFieldRangeFromBuffer(size_t startIndex, size_t endIndex);

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

        std::shared_ptr<const BufferObjectDefinition> m_definition;
        std::vector<uint8_t> m_buffer;  // CPU-side data
        IBufferMapping* m_mappedBuffer;  // GPU mapping (non-owning)

        friend class FieldProxy;
    };

    // ============================================================================
    // TEMPLATE IMPLEMENTATIONS
    // ============================================================================

    template<typename T>
    inline T BufferObjectInstance::Get(const std::string& path) const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        const FieldDescriptor* field = m_definition->FindField(path);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot get structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        return GetAtOffset<T>(field->offset);
    }

    template<typename T>
    inline void BufferObjectInstance::Set(const std::string& path, const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        const FieldDescriptor* field = m_definition->FindField(path);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot set structure field as value: " + path);
        }

        if (field->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + path);
        }

        SetAtOffset(field->offset, value);
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

} // namespace ShaderLib
