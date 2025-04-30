// UniformBufferManager.cpp
#include "UniformBufferManager.h"
#include <cassert>

UniformBufferManager::UniformBufferManager(VramManager& vramManager)
    : m_vramManager(vramManager) {
}

UniformBufferManager::~UniformBufferManager() {
    // Free all buffers
    for (auto& [handle, data] : m_buffers) {
        if (data.mappedMemory) {
            Buffer* buffer = m_vramManager.getResource<Buffer>(data.vramHandle);
            if (buffer) {
                buffer->unmap();
            }
        }
        m_vramManager.freeResource(data.vramHandle);
    }
}

UniformBufferHandle UniformBufferManager::createBuffer(const std::vector<Field>& fields, const std::string& debugName) {
    // Copy fields and calculate offsets
    std::vector<Field> alignedFields = fields;
    calculateOffsets(alignedFields);

    // Calculate total buffer size
    size_t totalSize = 0;
    if (!alignedFields.empty()) {
        const Field& lastField = alignedFields.back();
        totalSize = lastField.offset + lastField.size;
    }

    // Create handle
    UniformBufferHandle handle(m_nextId++);

    // Create buffer in VRAM - using the VramManager interface correctly
    VramHandle vramHandle = m_vramManager.createBuffer(
        totalSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (!vramHandle.isValid()) {
        return UniformBufferHandle(0); // Failed
    }

    // Prepare field index map for quick access
    std::unordered_map<std::string, size_t> fieldIndices;
    for (size_t i = 0; i < alignedFields.size(); ++i) {
        fieldIndices[alignedFields[i].name] = i;
    }

    // Save buffer data
    BufferData bufferData{
        vramHandle,
        std::move(alignedFields),
        std::move(fieldIndices),
        debugName,
        nullptr,
        true  // Needs update on start
    };

    // Optional: Persistent mapping for frequent updates
    Buffer* buffer = m_vramManager.getResource<Buffer>(vramHandle);
    if (buffer) {
        bufferData.mappedMemory = buffer->map();
    }

    m_buffers[handle] = std::move(bufferData);

    // Update buffer with initial values
    updateBuffer(handle);

    return handle;
}

void UniformBufferManager::updateBuffer(UniformBufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end() || !it->second.needsUpdate) {
        return;
    }

    BufferData& data = it->second;
    Buffer* buffer = m_vramManager.getResource<Buffer>(data.vramHandle);
    if (!buffer) {
        return;
    }

    // If we don't have persistent mapping, map memory temporarily
    void* mappedData = data.mappedMemory;
    bool tempMapping = false;

    if (!mappedData) {
        mappedData = buffer->map();
        tempMapping = true;
    }

    if (!mappedData) {
        return;  // Mapping failed
    }

    uint8_t* byteData = static_cast<uint8_t*>(mappedData);

    // Copy each field to buffer
    for (const auto& field : data.fields) {
        if (std::holds_alternative<float>(field.value)) {
            float value = std::get<float>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(float), &value, sizeof(float));
            }
        }
        else if (std::holds_alternative<glm::vec2>(field.value)) {
            glm::vec2 value = std::get<glm::vec2>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(glm::vec2), &value, sizeof(glm::vec2));
            }
        }
        else if (std::holds_alternative<glm::vec3>(field.value)) {
            glm::vec3 value = std::get<glm::vec3>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(glm::vec3), &value, sizeof(glm::vec3));
            }
        }
        else if (std::holds_alternative<glm::vec4>(field.value)) {
            glm::vec4 value = std::get<glm::vec4>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(glm::vec4), &value, sizeof(glm::vec4));
            }
        }
        else if (std::holds_alternative<int32_t>(field.value)) {
            int32_t value = std::get<int32_t>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(int32_t), &value, sizeof(int32_t));
            }
        }
        else if (std::holds_alternative<uint32_t>(field.value)) {
            uint32_t value = std::get<uint32_t>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(uint32_t), &value, sizeof(uint32_t));
            }
        }
        else if (std::holds_alternative<bool>(field.value)) {
            uint32_t value = std::get<bool>(field.value) ? 1 : 0;
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(uint32_t), &value, sizeof(uint32_t));
            }
        }
        else if (std::holds_alternative<glm::mat4>(field.value)) {
            glm::mat4 value = std::get<glm::mat4>(field.value);
            for (uint32_t i = 0; i < field.arraySize; i++) {
                memcpy(byteData + field.offset + i * sizeof(glm::mat4), &value, sizeof(glm::mat4));
            }
        }
    }

    // Unmap memory if it was temporary mapping
    if (tempMapping && buffer) {
        buffer->unmap();
    }

    data.needsUpdate = false;
}

void UniformBufferManager::updateField(UniformBufferHandle handle, const std::string& fieldName, const UniformValue& value) {
    auto bufferIt = m_buffers.find(handle);
    if (bufferIt == m_buffers.end()) {
        return;
    }

    BufferData& data = bufferIt->second;
    auto fieldIt = data.fieldIndices.find(fieldName);
    if (fieldIt == data.fieldIndices.end()) {
        return;
    }

    Field& field = data.fields[fieldIt->second];

    // Check type
    if (value.index() != field.value.index()) {
        return; // Incompatible type
    }

    field.value = value;
    data.needsUpdate = true;
}

void UniformBufferManager::updateFieldArray(UniformBufferHandle handle, const std::string& fieldName, const std::vector<UniformValue>& values) {
    auto bufferIt = m_buffers.find(handle);
    if (bufferIt == m_buffers.end()) {
        return;
    }

    BufferData& data = bufferIt->second;
    auto fieldIt = data.fieldIndices.find(fieldName);
    if (fieldIt == data.fieldIndices.end()) {
        return;
    }

    Field& field = data.fields[fieldIt->second];

    // Check if array has enough space
    if (values.empty() || values.size() > field.arraySize) {
        return;
    }

    // Check type of first element
    if (!values.empty() && values[0].index() != field.value.index()) {
        return; // Incompatible type
    }

    // Update individual elements in buffer
    Buffer* buffer = m_vramManager.getResource<Buffer>(data.vramHandle);
    if (!buffer) {
        return;
    }

    // If we don't have persistent mapping, map memory temporarily
    void* mappedData = data.mappedMemory;
    bool tempMapping = false;

    if (!mappedData) {
        mappedData = buffer->map();
        tempMapping = true;
    }

    if (!mappedData) {
        return;  // Mapping failed
    }

    uint8_t* byteData = static_cast<uint8_t*>(mappedData);
    size_t elementSize = getTypeSize(field.value);

    for (size_t i = 0; i < values.size(); i++) {
        const UniformValue& value = values[i];

        if (std::holds_alternative<float>(value)) {
            float val = std::get<float>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(float));
        }
        else if (std::holds_alternative<glm::vec2>(value)) {
            glm::vec2 val = std::get<glm::vec2>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(glm::vec2));
        }
        else if (std::holds_alternative<glm::vec3>(value)) {
            glm::vec3 val = std::get<glm::vec3>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(glm::vec3));
        }
        else if (std::holds_alternative<glm::vec4>(value)) {
            glm::vec4 val = std::get<glm::vec4>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(glm::vec4));
        }
        else if (std::holds_alternative<int32_t>(value)) {
            int32_t val = std::get<int32_t>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(int32_t));
        }
        else if (std::holds_alternative<uint32_t>(value)) {
            uint32_t val = std::get<uint32_t>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(uint32_t));
        }
        else if (std::holds_alternative<bool>(value)) {
            uint32_t val = std::get<bool>(value) ? 1 : 0;
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(uint32_t));
        }
        else if (std::holds_alternative<glm::mat4>(value)) {
            glm::mat4 val = std::get<glm::mat4>(value);
            memcpy(byteData + field.offset + i * elementSize, &val, sizeof(glm::mat4));
        }
    }

    // Unmap memory if it was temporary mapping
    if (tempMapping && buffer) {
        buffer->unmap();
    }
}

void UniformBufferManager::freeBuffer(UniformBufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        return;
    }

    BufferData& data = it->second;

    // Unmap memory if it was mapped
    if (data.mappedMemory) {
        Buffer* buffer = m_vramManager.getResource<Buffer>(data.vramHandle);
        if (buffer) {
            buffer->unmap();
        }
    }

    // Free VRAM
    m_vramManager.freeResource(data.vramHandle);

    // Remove from collection
    m_buffers.erase(it);
}

VkBuffer UniformBufferManager::getBuffer(UniformBufferHandle handle) const {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        return VK_NULL_HANDLE;
    }

    Buffer* buffer = m_vramManager.getResource<Buffer>(it->second.vramHandle);
    return buffer ? buffer->get() : VK_NULL_HANDLE;
}

VkDeviceSize UniformBufferManager::getBufferSize(UniformBufferHandle handle) const {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        return 0;
    }

    Buffer* buffer = m_vramManager.getResource<Buffer>(it->second.vramHandle);
    return buffer ? buffer->getSize() : 0;
}

const UniformBufferManager::Field* UniformBufferManager::getField(UniformBufferHandle handle, const std::string& fieldName) const {
    auto bufferIt = m_buffers.find(handle);
    if (bufferIt == m_buffers.end()) {
        return nullptr;
    }

    const BufferData& data = bufferIt->second;
    auto fieldIt = data.fieldIndices.find(fieldName);
    if (fieldIt == data.fieldIndices.end()) {
        return nullptr;
    }

    return &data.fields[fieldIt->second];
}

std::vector<UniformBufferManager::Field>* UniformBufferManager::getFields(UniformBufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        return nullptr;
    }

    return &it->second.fields;
}

size_t UniformBufferManager::calculateFieldSize(const Field& field) const {
    size_t elementSize = getTypeSize(field.value);
    return elementSize * field.arraySize;
}

size_t UniformBufferManager::alignSize(size_t size, size_t alignment) const {
    return (size + alignment - 1) & ~(alignment - 1);
}

void UniformBufferManager::calculateOffsets(std::vector<Field>& fields) const {
    size_t currentOffset = 0;

    for (auto& field : fields) {
        // Set size of single element
        size_t elementSize = getTypeSize(field.value);

        // Determine required alignment
        size_t alignment = elementSize;

        // Spec std140: vec3 has alignment like vec4
        if (std::holds_alternative<glm::vec3>(field.value)) {
            alignment = sizeof(glm::vec4);
        }

        // Arrays have special alignment rules in std140
        if (field.arraySize > 1) {
            // In std140, each array element must be aligned to 16 bytes
            alignment = std::max(alignment, size_t(16));
        }

        // Align offset
        currentOffset = alignSize(currentOffset, alignment);

        // Save offset
        field.offset = currentOffset;

        // Calculate and save size
        if (field.arraySize > 1) {
            // In std140, between array elements there's alignment to 16 bytes
            size_t arrayElementSize = alignSize(elementSize, size_t(16));
            field.size = arrayElementSize * field.arraySize;
        }
        else {
            field.size = elementSize;
        }

        // Increase offset for next field
        currentOffset += field.size;
    }
}

size_t UniformBufferManager::getTypeSize(const UniformValue& value) const {
    if (std::holds_alternative<float>(value)) {
        return sizeof(float);
    }
    else if (std::holds_alternative<glm::vec2>(value)) {
        return sizeof(glm::vec2);
    }
    else if (std::holds_alternative<glm::vec3>(value)) {
        return sizeof(glm::vec3);
    }
    else if (std::holds_alternative<glm::vec4>(value)) {
        return sizeof(glm::vec4);
    }
    else if (std::holds_alternative<int32_t>(value)) {
        return sizeof(int32_t);
    }
    else if (std::holds_alternative<uint32_t>(value)) {
        return sizeof(uint32_t);
    }
    else if (std::holds_alternative<bool>(value)) {
        return sizeof(uint32_t); // Bool in GLSL is 32 bits
    }
    else if (std::holds_alternative<glm::mat4>(value)) {
        return sizeof(glm::mat4);
    }

    return 0;
}