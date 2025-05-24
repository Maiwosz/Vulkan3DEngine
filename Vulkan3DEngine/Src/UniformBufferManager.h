#pragma once
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include "VramManager.h"
#include "ShaderLib.h"
#include "Buffer.h"
#include "Handle.h"
#include "ISmartHandleManager.h"

struct UniformBufferInfo {
    VramHandle vramHandle;
    std::string name;
    uint32_t size;
    bool inUse;
    uint32_t referenceCount;
    std::vector<ShaderLib::UniformVariable> variables;
};

class UniformBufferManager : public ISmartHandleManager<UniformBufferHandle, Buffer> {
public:
    explicit UniformBufferManager(VramManager& vramManager);
    ~UniformBufferManager();

    // Publiczny interfejs do ręcznego zarządzania
    UniformBufferHandle acquireBuffer(const ShaderLib::UniformBufferObject& uboInfo);
    void releaseBuffer(UniformBufferHandle handle);
    void updateBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset = 0);

    template<typename T>
    void updateVariable(UniformBufferHandle handle, const std::string& variableName, const T& value) {
        auto& bufferInfo = getBufferInfo(handle);

        for (const auto& variable : bufferInfo.variables) {
            if (variable.name == variableName) {
                if (sizeof(T) <= variable.size) {
                    updateBuffer(handle, &value, sizeof(T), variable.offset);
                    return;
                }
                SPDLOG_WARN("Variable size mismatch for '{}': expected {}, got {}",
                    variableName, variable.size, sizeof(T));
                return;
            }
        }

        SPDLOG_WARN("Variable '{}' not found in uniform buffer", variableName);
    }

    // Smart handle support - publiczny factory method
    SmartHandle<UniformBufferHandle, Buffer> acquireSmartBuffer(const ShaderLib::UniformBufferObject& uboInfo);

    // IResourceManager interface implementation
    Buffer* getResource(UniformBufferHandle handle) override;
    bool isValid(UniformBufferHandle handle) const override;
    void releaseResource(UniformBufferHandle handle) override;
    void addReference(UniformBufferHandle handle) override;
    void removeReference(UniformBufferHandle handle) override;

    void cleanupUnusedBuffers(uint64_t timeThreshold = 0);

    const UniformBufferInfo& getBufferInfo(UniformBufferHandle handle) const;
    UniformBufferInfo& getBufferInfo(UniformBufferHandle handle);

private:
    struct BufferPoolKey {
        std::string name;
        uint32_t size;

        bool operator==(const BufferPoolKey& other) const {
            return name == other.name && size == other.size;
        }
    };

    struct BufferPoolKeyHash {
        size_t operator()(const BufferPoolKey& key) const {
            return std::hash<std::string>()(key.name) ^ std::hash<uint32_t>()(key.size);
        }
    };

    // Prywatne metody zarządzania buforami
    UniformBufferHandle createNewBuffer(const ShaderLib::UniformBufferObject& uboInfo);
    UniformBufferHandle findReusableBuffer(const ShaderLib::UniformBufferObject& uboInfo);

    std::unordered_map<BufferPoolKey, std::deque<UniformBufferHandle>, BufferPoolKeyHash> m_bufferPool;
    std::unordered_map<UniformBufferHandle, UniformBufferInfo> m_buffers;
    VramManager& m_vramManager;
    uint32_t m_nextHandleId;
    std::mutex m_poolMutex;

    // Cache dla getResource (żeby zwrócić wskaźnik)
    mutable std::unordered_map<UniformBufferHandle, Buffer*> m_resourceCache;
};