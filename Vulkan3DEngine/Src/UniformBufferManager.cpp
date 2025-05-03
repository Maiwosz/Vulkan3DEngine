#include "UniformBufferManager.h"
#include <stdexcept>
#include <algorithm>

UniformBufferManager::UniformBufferManager(VramManager& vramManager)
    : m_vramManager(vramManager) {
}

UniformBufferManager::~UniformBufferManager() {
    // Zniszcz wszystkie bufory
    for (auto& [handle, info] : m_buffers) {
        m_vramManager.freeResource(info.vramHandle);
    }
    m_buffers.clear();
    m_bufferPool.clear();
}

UniformBufferHandle UniformBufferManager::createBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    // Tworzenie nowego uchwytu
    UniformBufferHandle handle(m_nextHandle++);

    // Tworzenie bufora uniform w VRAM
    VramHandle vramHandle = m_vramManager.createBuffer(
        uboInfo.size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Zapisanie informacji o buforze
    UniformBufferInfo bufferInfo;
    bufferInfo.vramHandle = vramHandle;
    bufferInfo.name = uboInfo.name;
    bufferInfo.size = uboInfo.size;
    bufferInfo.set = uboInfo.set;
    bufferInfo.binding = uboInfo.binding;
    bufferInfo.isInUse = true;
    bufferInfo.variables = uboInfo.variables;

    m_buffers[handle] = std::move(bufferInfo);

    return handle;
}

UniformBufferHandle UniformBufferManager::createGlobalBuffer(const ShaderLib::ShaderMetadata& metadata) {
    if (metadata.usesGlobalUBO) {
        return createBuffer(metadata.globalUBO);
    }
    return UniformBufferHandle(0);
}

UniformBufferHandle UniformBufferManager::createObjectBuffer(const ShaderLib::ShaderMetadata& metadata) {
    if (metadata.usesObjectUBO) {
        return createBuffer(metadata.objectUBO);
    }
    return UniformBufferHandle(0);
}

UniformBufferHandle UniformBufferManager::createCustomBuffer(const ShaderLib::ShaderMetadata& metadata, const std::string& name) {
    for (const auto& ubo : metadata.customUBOs) {
        if (ubo.name == name) {
            return createBuffer(ubo);
        }
    }
    return UniformBufferHandle(0);
}

UniformBufferHandle UniformBufferManager::acquireBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    // Klucz do identyfikacji bufora w puli
    BufferPoolKey key{ uboInfo.name, uboInfo.size, uboInfo.set, uboInfo.binding };

    // Sprawdź czy są dostępne bufory w puli
    auto& bufferPool = m_bufferPool[key];
    if (!bufferPool.empty()) {
        // Pobierz bufor z puli
        UniformBufferHandle handle = bufferPool.front();
        bufferPool.pop_front();

        // Oznacz bufor jako używany
        auto& bufferInfo = m_buffers[handle];
        bufferInfo.isInUse = true;

        return handle;
    }

    // Brak dostępnych buforów, utwórz nowy
    return createBuffer(uboInfo);
}

void UniformBufferManager::releaseBuffer(UniformBufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isBufferValid(handle)) {
        return;
    }

    // Pobierz informacje o buforze
    auto& bufferInfo = m_buffers[handle];

    // Oznacz bufor jako nieużywany
    bufferInfo.isInUse = false;

    // Dodaj bufor do puli
    BufferPoolKey key{ bufferInfo.name, bufferInfo.size, bufferInfo.set, bufferInfo.binding };
    m_bufferPool[key].push_back(handle);
}

void UniformBufferManager::updateBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset) {
    if (!isBufferValid(handle)) {
        return;
    }

    // Pobierz informacje o buforze
    auto& bufferInfo = m_buffers[handle];

    // Sprawdź czy offset i rozmiar są poprawne
    if (offset + size > bufferInfo.size) {
        // Obsługa błędu - wyjście poza zakres bufora
        throw std::out_of_range("Buffer update exceeds buffer size");
    }

    // Pobierz bufor z VramManager
    Buffer* buffer = m_vramManager.getResource<Buffer>(bufferInfo.vramHandle);
    if (!buffer) {
        throw std::runtime_error("Failed to get buffer resource");
    }

    // Mapuj bufor i zaktualizuj dane
    void* mappedData = buffer->map();
    if (mappedData) {
        char* dst = static_cast<char*>(mappedData) + offset;
        std::memcpy(dst, data, size);
        buffer->unmap();
    }
}

bool UniformBufferManager::isBufferValid(UniformBufferHandle handle) const {
    return m_buffers.find(handle) != m_buffers.end();
}

const UniformBufferInfo& UniformBufferManager::getBufferInfo(UniformBufferHandle handle) const {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        throw std::runtime_error("Invalid uniform buffer handle");
    }
    return it->second;
}

UniformBufferInfo& UniformBufferManager::getBufferInfo(UniformBufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        throw std::runtime_error("Invalid uniform buffer handle");
    }
    return it->second;
}

Buffer* UniformBufferManager::getBuffer(UniformBufferHandle handle) {
    if (!isBufferValid(handle)) {
        return nullptr;
    }

    // Pobierz bufor z VramManager
    return m_vramManager.getResource<Buffer>(m_buffers[handle].vramHandle);
}

void UniformBufferManager::cleanupUnusedBuffers(uint64_t timeThreshold) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    // Można tutaj dodać kod do usuwania starych buforów na podstawie timeThreshold
    // Dla prostoty, usuwamy tylko część nieużywanych buforów

    for (auto& [key, pool] : m_bufferPool) {
        // Zachowaj kilka buforów w puli, usuń resztę
        const size_t bufferesToKeep = 5; // Liczba buforów do zachowania w puli

        if (pool.size() > bufferesToKeep) {
            // Usuń nadmiarowe bufory
            size_t buffersToRemove = pool.size() - bufferesToKeep;

            for (size_t i = 0; i < buffersToRemove; ++i) {
                UniformBufferHandle handle = pool.back();
                pool.pop_back();

                // Zwolnij zasoby bufora
                m_vramManager.freeResource(m_buffers[handle].vramHandle);
                m_buffers.erase(handle);
            }
        }
    }
}