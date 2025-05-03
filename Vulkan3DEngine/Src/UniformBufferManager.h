#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <deque>
#include <mutex>
#include "VramManager.h"
#include "ShaderLib.h"
#include "Buffer.h"

// Handle do identyfikacji Uniform Buffer
struct UniformBufferHandle {
    uint32_t id;
    constexpr explicit UniformBufferHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const UniformBufferHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function dla UniformBufferHandle
namespace std {
    template <>
    struct hash<UniformBufferHandle> {
        size_t operator()(const UniformBufferHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}

// Struktura przechowująca informacje o buforze uniform
struct UniformBufferInfo {
    VramHandle vramHandle;
    std::string name;
    uint32_t size;
    uint32_t set;
    uint32_t binding;
    bool isInUse;
    std::vector<ShaderLib::UniformVariable> variables;
};

class UniformBufferManager {
public:
    explicit UniformBufferManager(VramManager& vramManager);
    ~UniformBufferManager();

    // Tworzenie bufora uniform na podstawie metadanych z shadera
    UniformBufferHandle createBuffer(const ShaderLib::UniformBufferObject& uboInfo);

    // Tworzenie konkretnego typu bufora uniform (globalny, obiektowy, niestandardowy)
    UniformBufferHandle createGlobalBuffer(const ShaderLib::ShaderMetadata& metadata);
    UniformBufferHandle createObjectBuffer(const ShaderLib::ShaderMetadata& metadata);
    UniformBufferHandle createCustomBuffer(const ShaderLib::ShaderMetadata& metadata, const std::string& name);

    // Pobieranie bufora uniform z puli (jeśli dostępny) lub tworzenie nowego
    UniformBufferHandle acquireBuffer(const ShaderLib::UniformBufferObject& uboInfo);

    // Zwracanie bufora uniform do puli
    void releaseBuffer(UniformBufferHandle handle);

    // Aktualizacja wartości w buforze uniform
    // Używanie offsetu i rozmiaru pozwala na aktualizację tylko części bufora
    void updateBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset = 0);

    // Aktualizacja konkretnej zmiennej w buforze uniform
    template<typename T>
    void updateVariable(UniformBufferHandle handle, const std::string& variableName, const T& value) {
        auto& bufferInfo = getBufferInfo(handle);

        // Znajdź zmienną po nazwie
        for (const auto& variable : bufferInfo.variables) {
            if (variable.name == variableName) {
                // Sprawdź czy rozmiar danych jest zgodny z rozmiarem zmiennej
                if (sizeof(T) <= variable.size) {
                    updateBuffer(handle, &value, sizeof(T), variable.offset);
                    return;
                }
            }
        }

        // Zmienna nie została znaleziona lub ma niepoprawny rozmiar
        // Możemy tutaj dodać obsługę błędów
    }

    // Sprawdzanie czy uchwyt do bufora uniform jest poprawny
    bool isBufferValid(UniformBufferHandle handle) const;

    // Pobieranie informacji o buforze uniform
    const UniformBufferInfo& getBufferInfo(UniformBufferHandle handle) const;
    UniformBufferInfo& getBufferInfo(UniformBufferHandle handle);

    // Pobieranie bufora z VramManager
    Buffer* getBuffer(UniformBufferHandle handle);

    // Czyszczenie nieużywanych buforów (opcjonalnie z limitem czasowym)
    void cleanupUnusedBuffers(uint64_t timeThreshold = 0);

private:
    // Klucz identyfikacyjny dla puli buforów
    struct BufferPoolKey {
        std::string name;
        uint32_t size;
        uint32_t set;
        uint32_t binding;

        bool operator==(const BufferPoolKey& other) const {
            return name == other.name &&
                size == other.size &&
                set == other.set &&
                binding == other.binding;
        }
    };

    // Hash function dla BufferPoolKey
    struct BufferPoolKeyHash {
        size_t operator()(const BufferPoolKey& key) const {
            return std::hash<std::string>()(key.name) ^
                std::hash<uint32_t>()(key.size) ^
                std::hash<uint32_t>()(key.set) ^
                std::hash<uint32_t>()(key.binding);
        }
    };

    // Pula buforów uniform
    std::unordered_map<BufferPoolKey, std::deque<UniformBufferHandle>, BufferPoolKeyHash> m_bufferPool;

    // Przechowywanie informacji o buforach uniform
    std::unordered_map<UniformBufferHandle, UniformBufferInfo> m_buffers;

    // Zasoby
    VramManager& m_vramManager;

    // Licznik dla generowania unikatowych uchwytów
    uint32_t m_nextHandle = 1;

    // Mutex do synchronizacji dostępu do puli buforów
    std::mutex m_poolMutex;
};