#pragma once
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include "VramManager.h"
#include "ShaderLib.h"
#include "ShaderTypes.h"
#include "BufferReader.h"
#include "BufferWriter.h"
#include "Buffer.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "MappedBufferReader.h"
#include "MappedBufferWriter.h"

struct BufferInfo {
    VramHandle vramHandle;
    std::string name;
    uint32_t size;
    ShaderLib::BufferType bufferType;
    ShaderLib::LayoutStandard layoutStandard;
    bool inUse;
    uint32_t referenceCount;

    // Cached BufferObject for Reader/Writer
    ShaderLib::BufferObject bufferObject;
};

// Universal manager for UBO and SSBO
class BufferManager : public ISmartHandleManager<BufferHandle, Buffer> {
public:
    explicit BufferManager(VramManager& vramManager);
    ~BufferManager();

    // Buffer lifecycle management
    BufferHandle acquireBuffer(const ShaderLib::BufferObject& bufferInfo);
    void releaseBuffer(BufferHandle handle);

    // Smart handle support
    SmartHandle<BufferHandle, Buffer> acquireSmartBuffer(const ShaderLib::BufferObject& bufferInfo);

    // Create RAII-wrapped Reader/Writer for a buffer (RECOMMENDED)
    MappedBufferReader createMappedReader(BufferHandle handle);
    MappedBufferWriter createMappedWriter(BufferHandle handle);

    // IResourceManager interface implementation
    Buffer* getResource(BufferHandle handle) override;
    bool isValid(BufferHandle handle) const override;
    void releaseResource(BufferHandle handle) override;
    void addReference(BufferHandle handle) override;
    void removeReference(BufferHandle handle) override;

    // Maintenance
    void cleanupUnusedBuffers(uint64_t timeThreshold = 0);

    // Info access
    const BufferInfo& getBufferInfo(BufferHandle handle) const;
    const ShaderLib::BufferObject& getBufferObject(BufferHandle handle) const;

private:
    struct BufferPoolKey {
        std::string name;
        uint32_t size;
        ShaderLib::BufferType bufferType;

        bool operator==(const BufferPoolKey& other) const {
            return name == other.name && size == other.size && bufferType == other.bufferType;
        }
    };

    struct BufferPoolKeyHash {
        size_t operator()(const BufferPoolKey& key) const {
            size_t h1 = std::hash<std::string>()(key.name);
            size_t h2 = std::hash<uint32_t>()(key.size);
            size_t h3 = std::hash<int>()(static_cast<int>(key.bufferType));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    BufferHandle createNewBuffer(const ShaderLib::BufferObject& bufferInfo);
    BufferHandle findReusableBuffer(const ShaderLib::BufferObject& bufferInfo);

    std::unordered_map<BufferPoolKey, std::deque<BufferHandle>, BufferPoolKeyHash> m_bufferPool;
    std::unordered_map<BufferHandle, BufferInfo> m_buffers;
    VramManager& m_vramManager;
    uint32_t m_nextHandleId;
    std::mutex m_poolMutex;

    mutable std::unordered_map<BufferHandle, Buffer*> m_resourceCache;
};