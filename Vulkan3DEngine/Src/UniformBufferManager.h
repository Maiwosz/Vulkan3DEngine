#pragma once
#include "VramManager.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <typeindex>

// Identyfikator uniform buffera
struct UniformBufferHandle {
    uint32_t id;
    constexpr explicit UniformBufferHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const UniformBufferHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<UniformBufferHandle> {
        size_t operator()(const UniformBufferHandle& h) const {
            return hash<uint32_t>()(h.id);
        }
    };
}

// Klasa zarządzająca uniform bufferami
class UniformBufferManager {
public:
    // Typy danych, które mogą być przechowywane w uniform bufferze
    using UniformValue = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        int32_t,
        uint32_t,
        bool,
        glm::mat4
    >;

    struct Field {
        std::string name;
        UniformValue value;
        uint32_t arraySize = 1;
        size_t offset = 0;  // Offset w buforze
        size_t size = 0;    // Rozmiar w bajtach
    };

    explicit UniformBufferManager(VramManager& vramManager);
    ~UniformBufferManager();

    // Tworzenie i zarządzanie bufferami
    UniformBufferHandle createBuffer(const std::vector<Field>& fields, const std::string& debugName = "");
    void updateBuffer(UniformBufferHandle handle);
    void updateField(UniformBufferHandle handle, const std::string& fieldName, const UniformValue& value);
    void updateFieldArray(UniformBufferHandle handle, const std::string& fieldName, const std::vector<UniformValue>& values);
    void freeBuffer(UniformBufferHandle handle);

    // Pobieranie informacji
    VkBuffer getBuffer(UniformBufferHandle handle) const;
    VkDeviceSize getBufferSize(UniformBufferHandle handle) const;
    const Field* getField(UniformBufferHandle handle, const std::string& fieldName) const;
    std::vector<Field>* getFields(UniformBufferHandle handle);

private:
    struct BufferData {
        VramHandle vramHandle;
        std::vector<Field> fields;
        std::unordered_map<std::string, size_t> fieldIndices;
        std::string debugName;
        void* mappedMemory = nullptr;  // For persistently mapped buffers
        bool needsUpdate = false;
    };

    VramManager& m_vramManager;
    std::unordered_map<UniformBufferHandle, BufferData> m_buffers;
    uint32_t m_nextId = 1;

    // Pomocnicze metody
    size_t calculateFieldSize(const Field& field) const;
    size_t alignSize(size_t size, size_t alignment) const;
    void calculateOffsets(std::vector<Field>& fields) const;
    size_t getTypeSize(const UniformValue& value) const;
};