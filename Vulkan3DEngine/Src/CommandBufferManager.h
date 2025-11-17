#pragma once
#include "VulkanContext.h"
#include "CommandBuffer.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <shared_mutex>

// Forward declarations
DEFINE_HANDLE_TYPE(CommandBufferHandle, uint32_t)
using SmartCommandBufferHandle = SmartHandle<CommandBufferHandle, CommandBuffer>;

class CommandBufferManager : public ISmartHandleManager<CommandBufferHandle, CommandBuffer> {
public:
    // Public alias for external use
    using SmartBuffer = SmartCommandBufferHandle;

    struct Configuration {
        QueueType queueType = QueueType::Graphics;
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VkCommandBufferUsageFlags usageFlags = 0;

        [[nodiscard]] bool operator==(const Configuration&) const = default;
    };

    explicit CommandBufferManager(VulkanContext& context);
    ~CommandBufferManager() override;

    // Smart handle factory methods
    [[nodiscard]] SmartCommandBufferHandle acquireSmartBuffer(const Configuration& config = {});

    // IResourceManager interface implementation
    CommandBuffer* getResource(CommandBufferHandle handle) override;
    bool isValid(CommandBufferHandle handle) const override;
    void releaseResource(CommandBufferHandle handle) override;
    void addReference(CommandBufferHandle handle) override;
    void removeReference(CommandBufferHandle handle) override;

    // Statistics and management
    [[nodiscard]] size_t getActiveBufferCount() const;
    [[nodiscard]] size_t getPooledBufferCount() const;
    void cleanup();

private:
    struct ConfigurationHash {
        [[nodiscard]] size_t operator()(const Configuration& config) const;
    };

    struct BufferEntry {
        std::unique_ptr<CommandBuffer> buffer;
        Configuration config;
        std::atomic<uint32_t> refCount{ 0 };
        bool isPooled = false;

        explicit BufferEntry(std::unique_ptr<CommandBuffer> buf, Configuration cfg)
            : buffer(std::move(buf)), config(std::move(cfg)) {
        }
    };

    using BufferPool = std::vector<std::unique_ptr<CommandBuffer>>;
    using BufferMap = std::unordered_map<CommandBufferHandle, std::unique_ptr<BufferEntry>>;
    using PoolMap = std::unordered_map<Configuration, BufferPool, ConfigurationHash>;

    [[nodiscard]] std::unique_ptr<CommandBuffer> createBuffer(const Configuration& config);
    [[nodiscard]] CommandBufferHandle generateHandle();
    void returnToPool(std::unique_ptr<CommandBuffer> buffer, const Configuration& config);

    VulkanContext& m_context;
    BufferMap m_activeBuffers;
    PoolMap m_pooledBuffers;
    mutable std::shared_mutex m_mutex;
    std::atomic<uint32_t> m_nextHandle{ 1 };
};
