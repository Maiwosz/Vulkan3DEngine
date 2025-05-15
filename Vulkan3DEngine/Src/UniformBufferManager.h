#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <deque>
#include <mutex>
#include "VramManager.h"
#include "ShaderLib.h"
#include "Buffer.h"
#include "Handle.h"

struct UniformBufferInfo {
    VramHandle vramHandle;
    std::string name;
    uint32_t size;
    bool isInUse;
    std::vector<ShaderLib::UniformVariable> variables;
};

class UniformBufferManager {
public:
    explicit UniformBufferManager(VramManager& vramManager);
    ~UniformBufferManager();

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

    bool isBufferValid(UniformBufferHandle handle) const;

    const UniformBufferInfo& getBufferInfo(UniformBufferHandle handle) const;
    UniformBufferInfo& getBufferInfo(UniformBufferHandle handle);

    Buffer* getBuffer(UniformBufferHandle handle);

    void cleanupUnusedBuffers(uint64_t timeThreshold = 0);

private:
    UniformBufferHandle createBuffer(const ShaderLib::UniformBufferObject& uboInfo);

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

    std::unordered_map<BufferPoolKey, std::deque<UniformBufferHandle>, BufferPoolKeyHash> m_bufferPool;
    std::unordered_map<UniformBufferHandle, UniformBufferInfo> m_buffers;
    VramManager& m_vramManager;
    uint32_t m_nextHandle = 1;
    std::mutex m_poolMutex;
};