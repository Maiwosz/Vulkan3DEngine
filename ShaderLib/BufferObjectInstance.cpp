#include "pch.h"
#include "BufferObjectInstance.h"
#include "FieldProxy.h"
#include "TypeSerializationTable.h"
#include <stdexcept>
#include <cstring>
#include <cassert>

namespace ShaderLib {

    // ============================================================================
    // CONSTRUCTION
    // ============================================================================

    BufferObjectInstance::BufferObjectInstance(
        std::shared_ptr<const BufferObjectDefinition> definition
    )
        : m_definition(definition)
        , m_mappedBuffer(nullptr)
    {
        if (!definition) {
            throw std::runtime_error("Buffer definition cannot be null");
        }

        InitializeBuffer();
    }

    void BufferObjectInstance::InitializeBuffer() {
        size_t bufferSize = m_definition->GetTotalSize();
        m_buffer.resize(bufferSize);

        std::memset(m_buffer.data(), 0, bufferSize);

        // Initialize matrices with identity values
        const auto& fields = m_definition->GetAllFields();
        for (const auto& field : fields) {
            if (!field.isBaseType) {
                continue;
            }

            switch (field.baseType) {
            case BaseType::Mat2:
                SetAtOffset(field.offset, glm::mat2(1.0f));
                break;
            case BaseType::Mat3:
                SetAtOffset(field.offset, glm::mat3(1.0f));
                break;
            case BaseType::Mat4:
                SetAtOffset(field.offset, glm::mat4(1.0f));
                break;
            default:
                break;
            }
        }
    }

    // ============================================================================
    // PROXY ACCESS
    // ============================================================================

    FieldProxy BufferObjectInstance::operator[](const std::string& name) {
        const auto& fields = m_definition->GetAllFields();
        for (const auto& field : fields) {
            if (field.name == name && field.parentPath.empty()) {
                return FieldProxy(this, &field);
            }
        }

        throw std::runtime_error("Field not found: " + name);
    }

    FieldProxy BufferObjectInstance::operator[](const char* name) {
        return operator[](std::string(name));
    }

    FieldProxy BufferObjectInstance::GetField(const std::string& path) {
        const FieldDescriptor* field = m_definition->FindField(path);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        return FieldProxy(this, field);
    }

    // ============================================================================
    // BULK OPERATIONS
    // ============================================================================

    void BufferObjectInstance::CopyFrom(const BufferObjectInstance& other) {
        if (m_definition != other.m_definition) {
            throw std::runtime_error(
                "Cannot copy between different buffer definitions"
            );
        }

        std::memcpy(m_buffer.data(), other.m_buffer.data(), m_buffer.size());
    }

    void BufferObjectInstance::CopyRegion(
        const BufferObjectInstance& other,
        uint32_t srcOffset,
        uint32_t dstOffset,
        uint32_t size
    ) {
        if (srcOffset + size > other.m_buffer.size()) {
            throw std::out_of_range("Source region out of range");
        }
        if (dstOffset + size > m_buffer.size()) {
            throw std::out_of_range("Destination region out of range");
        }

        std::memcpy(
            m_buffer.data() + dstOffset,
            other.m_buffer.data() + srcOffset,
            size
        );
    }

    void BufferObjectInstance::Zero() {
        std::memset(m_buffer.data(), 0, m_buffer.size());
    }

    void BufferObjectInstance::ZeroRegion(uint32_t offset, uint32_t size) {
        ValidateOffset(offset, size);
        std::memset(m_buffer.data() + offset, 0, size);
    }

    // ============================================================================
    // GPU BUFFER MAPPING
    // ============================================================================

    void BufferObjectInstance::SetMappedBuffer(IBufferMapping* buffer) {
        if (!buffer) {
            throw std::runtime_error("Cannot set null buffer");
        }

        m_mappedBuffer = buffer;

        if (!m_mappedBuffer->isMapped()) {
            void* ptr = m_mappedBuffer->map();
            if (!ptr) {
                throw std::runtime_error(
                    "Buffer is not persistently mapped and fallback mapping failed"
                );
            }
        }

        ValidateBufferSize();
    }

    bool BufferObjectInstance::IsBufferMapped() const {
        return m_mappedBuffer && m_mappedBuffer->isMapped();
    }

    void* BufferObjectInstance::GetMappedPointer() {
        if (!m_mappedBuffer) {
            throw std::runtime_error("No buffer assigned to this instance");
        }

        void* ptr = m_mappedBuffer->getMappedPointer();
        if (!ptr) {
            throw std::runtime_error(
                "Buffer is not mapped. Persistent mapping expected but not found."
            );
        }

        return ptr;
    }

    const void* BufferObjectInstance::GetMappedPointer() const {
        if (!m_mappedBuffer) {
            throw std::runtime_error("No buffer assigned to this instance");
        }

        const void* ptr = m_mappedBuffer->getMappedPointer();
        if (!ptr) {
            throw std::runtime_error(
                "Buffer is not mapped. Persistent mapping expected but not found."
            );
        }

        return ptr;
    }

    void BufferObjectInstance::ValidateBufferSize() const {
        if (!m_mappedBuffer) {
            return;
        }

        const size_t allocatedSize = m_mappedBuffer->getAllocatedSize();
        const size_t requiredSize = m_buffer.size();

        if (allocatedSize < requiredSize) {
            throw std::runtime_error(
                "Mapped buffer size (" + std::to_string(allocatedSize) +
                " bytes) is smaller than required buffer size (" +
                std::to_string(requiredSize) + " bytes)"
            );
        }
    }

    void BufferObjectInstance::ValidateSyncRange(uint32_t offset, uint32_t size) const {
        const size_t bufferSize = m_buffer.size();
        const size_t allocatedSize = m_mappedBuffer->getAllocatedSize();

        if (offset + size > bufferSize) {
            throw std::out_of_range(
                "Sync range [" + std::to_string(offset) + ", " +
                std::to_string(offset + size) + ") exceeds buffer size (" +
                std::to_string(bufferSize) + " bytes)"
            );
        }

        assert(offset + size <= allocatedSize &&
            "Sync range exceeds allocated buffer size");

        if (offset + size > allocatedSize) {
            throw std::runtime_error(
                "CRITICAL: Sync range exceeds allocated buffer size"
            );
        }
    }

    // ============================================================================
    // GPU SYNCHRONIZATION
    // ============================================================================

    void BufferObjectInstance::SyncToBuffer() {
        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();
        const size_t bufferSize = m_buffer.size();

        ValidateSyncRange(0, static_cast<uint32_t>(bufferSize));

        std::memcpy(gpuData, cpuData, bufferSize);
    }

    void BufferObjectInstance::SyncFromBuffer() {
        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();
        const size_t bufferSize = m_buffer.size();

        ValidateSyncRange(0, static_cast<uint32_t>(bufferSize));

        std::memcpy(cpuData, gpuData, bufferSize);
    }

    void BufferObjectInstance::SyncRangeToBuffer(uint32_t offset, uint32_t size) {
        ValidateSyncRange(offset, size);

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            static_cast<uint8_t*>(gpuData) + offset,
            cpuData + offset,
            size
        );
    }

    void BufferObjectInstance::SyncRangeFromBuffer(uint32_t offset, uint32_t size) {
        ValidateSyncRange(offset, size);

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            cpuData + offset,
            static_cast<const uint8_t*>(gpuData) + offset,
            size
        );
    }

    void BufferObjectInstance::SyncFieldToBuffer(const std::string& path) {
        const FieldDescriptor* field = m_definition->FindField(path);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot sync non-base-type field: " + path);
        }

        ValidateSyncRange(field->offset, field->size);

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            static_cast<uint8_t*>(gpuData) + field->offset,
            cpuData + field->offset,
            field->size
        );
    }

    void BufferObjectInstance::SyncFieldFromBuffer(const std::string& path) {
        const FieldDescriptor* field = m_definition->FindField(path);
        if (!field) {
            throw std::runtime_error("Field not found: " + path);
        }

        if (!field->isBaseType) {
            throw std::runtime_error("Cannot sync non-base-type field: " + path);
        }

        ValidateSyncRange(field->offset, field->size);

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            cpuData + field->offset,
            static_cast<const uint8_t*>(gpuData) + field->offset,
            field->size
        );
    }

    void BufferObjectInstance::SyncFieldsToBuffer(const std::vector<std::string>& paths) {
        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        for (const auto& path : paths) {
            const FieldDescriptor* field = m_definition->FindField(path);
            if (!field || !field->isBaseType) {
                continue;
            }

            ValidateSyncRange(field->offset, field->size);

            std::memcpy(
                static_cast<uint8_t*>(gpuData) + field->offset,
                cpuData + field->offset,
                field->size
            );
        }
    }

    void BufferObjectInstance::SyncFieldsFromBuffer(const std::vector<std::string>& paths) {
        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        for (const auto& path : paths) {
            const FieldDescriptor* field = m_definition->FindField(path);
            if (!field || !field->isBaseType) {
                continue;
            }

            ValidateSyncRange(field->offset, field->size);

            std::memcpy(
                cpuData + field->offset,
                static_cast<const uint8_t*>(gpuData) + field->offset,
                field->size
            );
        }
    }

    void BufferObjectInstance::SyncFieldToBufferByIndex(size_t fieldIndex) {
        const auto& allFields = m_definition->GetAllFields();
        if (fieldIndex >= allFields.size()) {
            throw std::out_of_range("Field index out of range");
        }

        const auto& field = allFields[fieldIndex];
        if (!field.isBaseType) {
            throw std::runtime_error(
                "Cannot sync non-base-type field at index: " +
                std::to_string(fieldIndex)
            );
        }

        ValidateSyncRange(field.offset, field.size);

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            static_cast<uint8_t*>(gpuData) + field.offset,
            cpuData + field.offset,
            field.size
        );
    }

    void BufferObjectInstance::SyncFieldFromBufferByIndex(size_t fieldIndex) {
        const auto& allFields = m_definition->GetAllFields();
        if (fieldIndex >= allFields.size()) {
            throw std::out_of_range("Field index out of range");
        }

        const auto& field = allFields[fieldIndex];
        if (!field.isBaseType) {
            throw std::runtime_error(
                "Cannot sync non-base-type field at index: " +
                std::to_string(fieldIndex)
            );
        }

        ValidateSyncRange(field.offset, field.size);

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        std::memcpy(
            cpuData + field.offset,
            static_cast<const uint8_t*>(gpuData) + field.offset,
            field.size
        );
    }

    void BufferObjectInstance::SyncFieldsToBufferByIndices(
        const std::vector<size_t>& fieldIndices
    ) {
        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();
        const auto& allFields = m_definition->GetAllFields();

        for (size_t fieldIndex : fieldIndices) {
            if (fieldIndex >= allFields.size()) {
                continue;
            }

            const auto& field = allFields[fieldIndex];
            if (!field.isBaseType) {
                continue;
            }

            ValidateSyncRange(field.offset, field.size);

            std::memcpy(
                static_cast<uint8_t*>(gpuData) + field.offset,
                cpuData + field.offset,
                field.size
            );
        }
    }

    void BufferObjectInstance::SyncFieldsFromBufferByIndices(
        const std::vector<size_t>& fieldIndices
    ) {
        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();
        const auto& allFields = m_definition->GetAllFields();

        for (size_t fieldIndex : fieldIndices) {
            if (fieldIndex >= allFields.size()) {
                continue;
            }

            const auto& field = allFields[fieldIndex];
            if (!field.isBaseType) {
                continue;
            }

            ValidateSyncRange(field.offset, field.size);

            std::memcpy(
                cpuData + field.offset,
                static_cast<const uint8_t*>(gpuData) + field.offset,
                field.size
            );
        }
    }

    void BufferObjectInstance::SyncFieldRangeToBuffer(
        size_t startIndex,
        size_t endIndex
    ) {
        const auto& allFields = m_definition->GetAllFields();
        if (startIndex >= allFields.size()) {
            throw std::out_of_range("Start index out of range");
        }
        if (endIndex > allFields.size()) {
            throw std::out_of_range("End index out of range");
        }
        if (startIndex >= endIndex) {
            throw std::invalid_argument("Start index must be less than end index");
        }

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        for (size_t i = startIndex; i < endIndex; ++i) {
            const auto& field = allFields[i];
            if (!field.isBaseType) {
                continue;
            }

            ValidateSyncRange(field.offset, field.size);

            std::memcpy(
                static_cast<uint8_t*>(gpuData) + field.offset,
                cpuData + field.offset,
                field.size
            );
        }
    }

    void BufferObjectInstance::SyncFieldRangeFromBuffer(
        size_t startIndex,
        size_t endIndex
    ) {
        const auto& allFields = m_definition->GetAllFields();
        if (startIndex >= allFields.size()) {
            throw std::out_of_range("Start index out of range");
        }
        if (endIndex > allFields.size()) {
            throw std::out_of_range("End index out of range");
        }
        if (startIndex >= endIndex) {
            throw std::invalid_argument("Start index must be less than end index");
        }

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        for (size_t i = startIndex; i < endIndex; ++i) {
            const auto& field = allFields[i];
            if (!field.isBaseType) {
                continue;
            }

            ValidateSyncRange(field.offset, field.size);

            std::memcpy(
                cpuData + field.offset,
                static_cast<const uint8_t*>(gpuData) + field.offset,
                field.size
            );
        }
    }

    // ============================================================================
    // CLONING
    // ============================================================================

    std::shared_ptr<BufferObjectInstance> BufferObjectInstance::Clone() const {
        auto clone = std::make_shared<BufferObjectInstance>(m_definition);
        clone->CopyFrom(*this);
        return clone;
    }

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    json BufferObjectInstance::ToJson() const {
        json j;

        j["definition"] = m_definition->ToJson();

        json fieldsJson = json::object();
        const auto& fields = m_definition->GetAllFields();

        for (const auto& field : fields) {
            if (!field.isBaseType) {
                continue;
            }

            const auto& serInfo = GetSerializationInfo(field.baseType);

            if (!serInfo.SupportsJson()) {
                continue;
            }

            BaseTypeValue value = serInfo.readFromBuffer(m_buffer.data() + field.offset);
            fieldsJson[field.path] = serInfo.toJson(value);
        }

        j["fields"] = fieldsJson;
        return j;
    }

    bool BufferObjectInstance::FromJson(const json& j) {
        try {
            if (!j.contains("fields")) {
                return false;
            }

            const auto& fieldsJson = j.at("fields");
            const auto& fields = m_definition->GetAllFields();

            for (const auto& field : fields) {
                if (!field.isBaseType) {
                    continue;
                }

                if (!fieldsJson.contains(field.path)) {
                    continue;
                }

                const json& valueJson = fieldsJson.at(field.path);
                const auto& serInfo = GetSerializationInfo(field.baseType);

                if (!serInfo.SupportsJson()) {
                    continue;
                }

                BaseTypeValue value = serInfo.fromJson(valueJson);
                serInfo.writeToFixedBuffer(m_buffer.data() + field.offset, value);
            }

            return true;
        }
        catch (const std::exception&) {
            return false;
        }
    }

    std::shared_ptr<BufferObjectInstance> BufferObjectInstance::FromJson(
        const json& j,
        std::shared_ptr<const BufferObjectDefinition> definition
    ) {
        if (!definition) {
            throw std::runtime_error("Definition cannot be null");
        }

        auto instance = std::make_shared<BufferObjectInstance>(definition);

        if (!instance->FromJson(j)) {
            throw std::runtime_error("Failed to deserialize buffer instance");
        }

        return instance;
    }

    void BufferObjectInstance::ValidateOffset(uint32_t offset, uint32_t size) const {
        if (offset + size > m_buffer.size()) {
            throw std::out_of_range("Buffer access out of range");
        }
    }

} // namespace ShaderLib
