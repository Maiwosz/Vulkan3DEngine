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
        , m_asyncOps(nullptr)
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
    // BULK OPERATIONS (blocking)
    // ============================================================================

    void BufferObjectInstance::CopyFrom(const BufferObjectInstance& other) {
        if (m_definition != other.m_definition) {
            throw std::runtime_error(
                "Cannot copy between different buffer definitions"
            );
        }

        if (m_asyncOps) {
            // Use async chunked copy
            auto handle = m_asyncOps->ExecuteChunked(
                m_buffer.size(),
                [this, &other](size_t offset, size_t size) {
                    std::memcpy(
                        m_buffer.data() + offset,
                        other.m_buffer.data() + offset,
                        size
                    );
                }
            );
            m_asyncOps->WaitForOperation(handle);
        }
        else {
            // Fallback to synchronous copy
            std::memcpy(m_buffer.data(), other.m_buffer.data(), m_buffer.size());
        }
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

        if (m_asyncOps) {
            // Use async chunked copy for large regions
            constexpr size_t MIN_ASYNC_SIZE = 4096;

            if (size >= MIN_ASYNC_SIZE) {
                auto handle = m_asyncOps->ExecuteChunked(
                    size,
                    [this, &other, srcOffset, dstOffset](size_t offset, size_t chunkSize) {
                        std::memcpy(
                            m_buffer.data() + dstOffset + offset,
                            other.m_buffer.data() + srcOffset + offset,
                            chunkSize
                        );
                    }
                );
                m_asyncOps->WaitForOperation(handle);
                return;
            }
        }

        // Fallback to synchronous copy for small regions
        std::memcpy(
            m_buffer.data() + dstOffset,
            other.m_buffer.data() + srcOffset,
            size
        );
    }

    void BufferObjectInstance::Zero() {
        if (m_asyncOps) {
            // Use async chunked zero
            auto handle = m_asyncOps->ExecuteChunked(
                m_buffer.size(),
                [this](size_t offset, size_t size) {
                    std::memset(m_buffer.data() + offset, 0, size);
                }
            );
            m_asyncOps->WaitForOperation(handle);
        }
        else {
            // Fallback to synchronous zero
            std::memset(m_buffer.data(), 0, m_buffer.size());
        }
    }

    void BufferObjectInstance::ZeroRegion(uint32_t offset, uint32_t size) {
        ValidateOffset(offset, size);

        if (m_asyncOps) {
            // Use async chunked zero for large regions
            constexpr size_t MIN_ASYNC_SIZE = 4096;

            if (size >= MIN_ASYNC_SIZE) {
                auto handle = m_asyncOps->ExecuteChunked(
                    size,
                    [this, offset](size_t chunkOffset, size_t chunkSize) {
                        std::memset(
                            m_buffer.data() + offset + chunkOffset,
                            0,
                            chunkSize
                        );
                    }
                );
                m_asyncOps->WaitForOperation(handle);
                return;
            }
        }

        // Fallback to synchronous zero
        std::memset(m_buffer.data() + offset, 0, size);
    }

    // ============================================================================
    // BULK OPERATIONS (non-blocking)
    // ============================================================================

    AsyncOperationHandle BufferObjectInstance::CopyFromAsync(
        const BufferObjectInstance& other
    ) {
        ValidateAsyncAvailable();

        if (m_definition != other.m_definition) {
            throw std::runtime_error(
                "Cannot copy between different buffer definitions"
            );
        }

        return m_asyncOps->ExecuteChunked(
            m_buffer.size(),
            [this, &other](size_t offset, size_t size) {
                std::memcpy(
                    m_buffer.data() + offset,
                    other.m_buffer.data() + offset,
                    size
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::CopyRegionAsync(
        const BufferObjectInstance& other,
        uint32_t srcOffset,
        uint32_t dstOffset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();

        if (srcOffset + size > other.m_buffer.size()) {
            throw std::out_of_range("Source region out of range");
        }
        if (dstOffset + size > m_buffer.size()) {
            throw std::out_of_range("Destination region out of range");
        }

        return m_asyncOps->ExecuteChunked(
            size,
            [this, &other, srcOffset, dstOffset](size_t offset, size_t chunkSize) {
                std::memcpy(
                    m_buffer.data() + dstOffset + offset,
                    other.m_buffer.data() + srcOffset + offset,
                    chunkSize
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::ZeroAsync() {
        ValidateAsyncAvailable();

        return m_asyncOps->ExecuteChunked(
            m_buffer.size(),
            [this](size_t offset, size_t size) {
                std::memset(m_buffer.data() + offset, 0, size);
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::ZeroRegionAsync(
        uint32_t offset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();
        ValidateOffset(offset, size);

        return m_asyncOps->ExecuteChunked(
            size,
            [this, offset](size_t chunkOffset, size_t chunkSize) {
                std::memset(
                    m_buffer.data() + offset + chunkOffset,
                    0,
                    chunkSize
                );
            }
        );
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

    // ============================================================================
    // GPU SYNCHRONIZATION (blocking)
    // ============================================================================

    void BufferObjectInstance::SyncToBuffer() {
        if (!HasMappedBuffer()) {
            return;
        }

        const size_t bufferSize = m_buffer.size();
        SyncToBufferInternal(0, static_cast<uint32_t>(bufferSize));
        m_cpuBufferValid = true;
    }

    void BufferObjectInstance::SyncFromBuffer() {
        if (!HasMappedBuffer()) {
            return;
        }

        const size_t bufferSize = m_buffer.size();
        SyncFromBufferInternal(0, static_cast<uint32_t>(bufferSize));
        m_cpuBufferValid = true;
    }

    void BufferObjectInstance::SyncRangeToBuffer(uint32_t offset, uint32_t size) {
        if (!HasMappedBuffer()) {
            return;
        }

        ValidateSyncRange(offset, size);
        SyncToBufferInternal(offset, size);
    }

    void BufferObjectInstance::SyncRangeFromBuffer(uint32_t offset, uint32_t size) {
        if (!HasMappedBuffer()) {
            return;
        }

        ValidateSyncRange(offset, size);
        SyncFromBufferInternal(offset, size);
    }

    void BufferObjectInstance::SyncToBufferInternal(uint32_t offset, uint32_t size) {
        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        if (m_asyncOps) {
            // Use async chunked copy
            auto handle = m_asyncOps->ExecuteChunked(
                size,
                [gpuData, cpuData, offset](size_t chunkOffset, size_t chunkSize) {
                    const size_t absoluteOffset = offset + chunkOffset;
                    std::memcpy(
                        static_cast<uint8_t*>(gpuData) + absoluteOffset,
                        cpuData + absoluteOffset,
                        chunkSize
                    );
                }
            );
            m_asyncOps->WaitForOperation(handle);
        }
        else {
            // Fallback to synchronous copy
            std::memcpy(
                static_cast<uint8_t*>(gpuData) + offset,
                cpuData + offset,
                size
            );
        }
    }

    void BufferObjectInstance::SyncFromBufferInternal(uint32_t offset, uint32_t size) {
        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        if (m_asyncOps) {
            // Use async chunked copy
            auto handle = m_asyncOps->ExecuteChunked(
                size,
                [gpuData, cpuData, offset](size_t chunkOffset, size_t chunkSize) {
                    const size_t absoluteOffset = offset + chunkOffset;
                    std::memcpy(
                        cpuData + absoluteOffset,
                        static_cast<const uint8_t*>(gpuData) + absoluteOffset,
                        chunkSize
                    );
                }
            );
            m_asyncOps->WaitForOperation(handle);
        }
        else {
            // Fallback to synchronous copy
            std::memcpy(
                cpuData + offset,
                static_cast<const uint8_t*>(gpuData) + offset,
                size
            );
        }
    }

    // ============================================================================
    // ASYNC GPU SYNCHRONIZATION (non-blocking)
    // ============================================================================

    AsyncOperationHandle BufferObjectInstance::SyncToBufferAsync() {
        ValidateAsyncAvailable();

        if (!HasMappedBuffer()) {
            return AsyncOperationHandle();
        }

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();
        const size_t bufferSize = m_buffer.size();

        ValidateSyncRange(0, static_cast<uint32_t>(bufferSize));

        return m_asyncOps->ExecuteChunked(
            bufferSize,
            [gpuData, cpuData](size_t offset, size_t size) {
                std::memcpy(
                    static_cast<uint8_t*>(gpuData) + offset,
                    cpuData + offset,
                    size
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::SyncFromBufferAsync() {
        ValidateAsyncAvailable();

        if (!HasMappedBuffer()) {
            return AsyncOperationHandle();
        }

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();
        const size_t bufferSize = m_buffer.size();

        ValidateSyncRange(0, static_cast<uint32_t>(bufferSize));

        return m_asyncOps->ExecuteChunked(
            bufferSize,
            [gpuData, cpuData](size_t offset, size_t size) {
                std::memcpy(
                    cpuData + offset,
                    static_cast<const uint8_t*>(gpuData) + offset,
                    size
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::SyncRangeToBufferAsync(
        uint32_t offset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();
        ValidateSyncRange(offset, size);

        if (!HasMappedBuffer()) {
            return AsyncOperationHandle();
        }

        void* gpuData = GetMappedPointer();
        const uint8_t* cpuData = m_buffer.data();

        return m_asyncOps->ExecuteChunked(
            size,
            [gpuData, cpuData, offset](size_t chunkOffset, size_t chunkSize) {
                const size_t absoluteOffset = offset + chunkOffset;
                std::memcpy(
                    static_cast<uint8_t*>(gpuData) + absoluteOffset,
                    cpuData + absoluteOffset,
                    chunkSize
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::SyncRangeFromBufferAsync(
        uint32_t offset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();
        ValidateSyncRange(offset, size);

        if (!HasMappedBuffer()) {
            return AsyncOperationHandle();
        }

        const void* gpuData = GetMappedPointer();
        uint8_t* cpuData = m_buffer.data();

        return m_asyncOps->ExecuteChunked(
            size,
            [gpuData, cpuData, offset](size_t chunkOffset, size_t chunkSize) {
                const size_t absoluteOffset = offset + chunkOffset;
                std::memcpy(
                    cpuData + absoluteOffset,
                    static_cast<const uint8_t*>(gpuData) + absoluteOffset,
                    chunkSize
                );
            }
        );
    }

    // ============================================================================
    // DIRECT GPU ACCESS (blocking)
    // ============================================================================

    void BufferObjectInstance::CopyToGPUDirect(
        const void* source,
        uint32_t offset,
        uint32_t size
    ) {
        ValidateOffset(offset, size);
        ValidateGPUAccess();

        void* gpuData = GetMappedPointer();

        if (m_asyncOps) {
            // Use async chunked copy for large transfers
            constexpr size_t MIN_ASYNC_SIZE = 4096;

            if (size >= MIN_ASYNC_SIZE) {
                auto handle = m_asyncOps->ExecuteChunked(
                    size,
                    [gpuData, source, offset](size_t chunkOffset, size_t chunkSize) {
                        std::memcpy(
                            static_cast<uint8_t*>(gpuData) + offset + chunkOffset,
                            static_cast<const uint8_t*>(source) + chunkOffset,
                            chunkSize
                        );
                    }
                );
                m_asyncOps->WaitForOperation(handle);
                m_cpuBufferValid = false;
                return;
            }
        }

        // Fallback to synchronous copy
        std::memcpy(static_cast<uint8_t*>(gpuData) + offset, source, size);
        m_cpuBufferValid = false;
    }

    void BufferObjectInstance::CopyFromGPUDirect(
        void* destination,
        uint32_t offset,
        uint32_t size
    ) {
        ValidateOffset(offset, size);
        ValidateGPUAccess();

        const void* gpuData = GetMappedPointer();

        if (m_asyncOps) {
            // Use async chunked copy for large transfers
            constexpr size_t MIN_ASYNC_SIZE = 4096;

            if (size >= MIN_ASYNC_SIZE) {
                auto handle = m_asyncOps->ExecuteChunked(
                    size,
                    [gpuData, destination, offset](size_t chunkOffset, size_t chunkSize) {
                        std::memcpy(
                            static_cast<uint8_t*>(destination) + chunkOffset,
                            static_cast<const uint8_t*>(gpuData) + offset + chunkOffset,
                            chunkSize
                        );
                    }
                );
                m_asyncOps->WaitForOperation(handle);
                return;
            }
        }

        // Fallback to synchronous copy
        std::memcpy(destination, static_cast<const uint8_t*>(gpuData) + offset, size);
    }

    // ============================================================================
    // DIRECT GPU ACCESS (non-blocking)
    // ============================================================================

    AsyncOperationHandle BufferObjectInstance::CopyToGPUDirectAsync(
        const void* source,
        uint32_t offset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();
        ValidateOffset(offset, size);
        ValidateGPUAccess();

        void* gpuData = GetMappedPointer();
        m_cpuBufferValid = false;

        return m_asyncOps->ExecuteChunked(
            size,
            [gpuData, source, offset](size_t chunkOffset, size_t chunkSize) {
                std::memcpy(
                    static_cast<uint8_t*>(gpuData) + offset + chunkOffset,
                    static_cast<const uint8_t*>(source) + chunkOffset,
                    chunkSize
                );
            }
        );
    }

    AsyncOperationHandle BufferObjectInstance::CopyFromGPUDirectAsync(
        void* destination,
        uint32_t offset,
        uint32_t size
    ) {
        ValidateAsyncAvailable();
        ValidateOffset(offset, size);
        ValidateGPUAccess();

        const void* gpuData = GetMappedPointer();

        return m_asyncOps->ExecuteChunked(
            size,
            [gpuData, destination, offset](size_t chunkOffset, size_t chunkSize) {
                std::memcpy(
                    static_cast<uint8_t*>(destination) + chunkOffset,
                    static_cast<const uint8_t*>(gpuData) + offset + chunkOffset,
                    chunkSize
                );
            }
        );
    }

    // ============================================================================
    // ASYNC OPERATION MANAGEMENT
    // ============================================================================

    bool BufferObjectInstance::IsSyncComplete(AsyncOperationHandle handle) {
        if (!m_asyncOps) {
            return true;
        }
        return m_asyncOps->IsOperationComplete(handle);
    }

    void BufferObjectInstance::WaitForSync(AsyncOperationHandle handle) {
        if (!m_asyncOps) {
            return;
        }
        m_asyncOps->WaitForOperation(handle);
        m_cpuBufferValid = true;
    }

    void BufferObjectInstance::WaitForAllSyncs() {
        if (!m_asyncOps) {
            return;
        }
        m_asyncOps->WaitForAll();
        m_cpuBufferValid = true;
    }

    // ============================================================================
    // ASYNC OPERATIONS INTERFACE
    // ============================================================================

    void BufferObjectInstance::SetAsyncOperations(IAsyncMemoryOperations* asyncOps) {
        m_asyncOps = asyncOps;
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

    // ============================================================================
    // VALIDATION
    // ============================================================================

    void BufferObjectInstance::ValidateOffset(uint32_t offset, uint32_t size) const {
        if (offset + size > m_buffer.size()) {
            throw std::out_of_range("Buffer access out of range");
        }
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

    void BufferObjectInstance::ValidateGPUAccess() const {
        if (!m_mappedBuffer) {
            throw std::runtime_error("No GPU buffer mapped");
        }

        if (!m_mappedBuffer->isMapped()) {
            throw std::runtime_error("GPU buffer is not mapped");
        }
    }

    void BufferObjectInstance::ValidateAsyncAvailable() const {
        if (!m_asyncOps) {
            throw std::runtime_error(
                "Async operations not available. Set IAsyncMemoryOperations interface first."
            );
        }
    }

} // namespace ShaderLib
