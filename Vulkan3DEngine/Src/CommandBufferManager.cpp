#include "CommandBufferManager.h"
#include <functional>
#include <stdexcept>
#include <format>

size_t CommandBufferManager::ConfigurationHash::operator()(const Configuration& config) const {
    size_t seed = 0;

    // Use std::hash and combine hashes properly
    auto hashCombine = [](size_t& seed, size_t hash) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

    hashCombine(seed, std::hash<int>{}(static_cast<int>(config.queueType)));
    hashCombine(seed, std::hash<VkCommandBufferLevel>{}(config.level));
    hashCombine(seed, std::hash<VkCommandBufferUsageFlags>{}(config.usageFlags));

    return seed;
}

CommandBufferManager::CommandBufferManager(VulkanContext& context)
    : m_context(context) {
}

CommandBufferManager::~CommandBufferManager() {
    cleanup();
}

SmartCommandBufferHandle CommandBufferManager::acquireSmartBuffer(const Configuration& config) {
    auto handle = generateHandle();
    auto buffer = createBuffer(config);

    if (!buffer || !buffer->isValid()) {
        throw std::runtime_error("Failed to create command buffer");
    }

    {
        std::unique_lock lock(m_mutex);
        auto entry = std::make_unique<BufferEntry>(std::move(buffer), config);
        m_activeBuffers[handle] = std::move(entry);
    }

    return createSmartHandle(handle);
}

CommandBuffer* CommandBufferManager::getResource(CommandBufferHandle handle) {
    std::shared_lock lock(m_mutex);

    if (auto it = m_activeBuffers.find(handle); it != m_activeBuffers.end()) {
        return it->second->buffer.get();
    }

    return nullptr;
}

bool CommandBufferManager::isValid(CommandBufferHandle handle) const {
    if (!handle.isValid()) {
        return false;
    }

    std::shared_lock lock(m_mutex);

    if (auto it = m_activeBuffers.find(handle); it != m_activeBuffers.end()) {
        return it->second->buffer && it->second->buffer->isValid();
    }

    return false;
}

void CommandBufferManager::releaseResource(CommandBufferHandle handle) {
    std::unique_lock lock(m_mutex);

    if (auto it = m_activeBuffers.find(handle); it != m_activeBuffers.end()) {
        auto& entry = it->second;

        // Return buffer to pool for reuse if it's in good state
        if (entry->buffer && entry->buffer->isValid()) {
            try {
                entry->buffer->reset();
                returnToPool(std::move(entry->buffer), entry->config);
            }
            catch (const std::exception&) {
                // If reset fails, just let the buffer be destroyed
            }
        }

        m_activeBuffers.erase(it);
    }
}

void CommandBufferManager::addReference(CommandBufferHandle handle) {
    std::shared_lock lock(m_mutex);

    if (auto it = m_activeBuffers.find(handle); it != m_activeBuffers.end()) {
        it->second->refCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void CommandBufferManager::removeReference(CommandBufferHandle handle) {
    {
        std::shared_lock lock(m_mutex);

        if (auto it = m_activeBuffers.find(handle); it != m_activeBuffers.end()) {
            if (it->second->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // Last reference removed, need to release
                lock.unlock();
                releaseResource(handle);
                return;
            }
        }
    }
}

std::unique_ptr<CommandBuffer> CommandBufferManager::createBuffer(const Configuration& config) {
    // Try to get from pool first
    {
        std::unique_lock lock(m_mutex);

        if (auto poolIt = m_pooledBuffers.find(config); poolIt != m_pooledBuffers.end()) {
            auto& pool = poolIt->second;
            if (!pool.empty()) {
                auto buffer = std::move(pool.back());
                pool.pop_back();

                // Reset the buffer to ensure clean state
                try {
                    buffer->reset();
                    return buffer;
                }
                catch (const std::exception&) {
                    // If reset fails, fall through to create new buffer
                }
            }
        }
    }

    // Create new buffer - pobierz odpowiedni pool który już wie jaki typ kolejki obsługuje
    CommandPool* pool = nullptr;

    switch (config.queueType) {
    case QueueType::Graphics:
        pool = &m_context.graphicsCommandPool();
        break;
    case QueueType::Transfer:
        pool = &m_context.transferCommandPool();
        break;
    case QueueType::Compute:
        pool = &m_context.computeCommandPool();
        break;
    default:
        throw std::invalid_argument("Invalid queue type for command buffer");
    }

    // CommandPool już wie jaki typ kolejki obsługuje, przekaż go do CommandBuffer
    return std::make_unique<CommandBuffer>(
        std::shared_ptr<CommandPool>(pool, [](CommandPool*) {}),
        pool->getQueueType(),
        config.level
    );
}

CommandBufferHandle CommandBufferManager::generateHandle() {
    return CommandBufferHandle{ m_nextHandle.fetch_add(1, std::memory_order_relaxed) };
}

void CommandBufferManager::returnToPool(std::unique_ptr<CommandBuffer> buffer, const Configuration& config) {
    if (!buffer) {
        return;
    }

    // Note: This function is called from within a unique_lock, so no additional locking needed
    constexpr size_t maxPoolSize = 16; // Limit pool size to prevent excessive memory usage

    auto& pool = m_pooledBuffers[config];
    if (pool.size() < maxPoolSize) {
        pool.emplace_back(std::move(buffer));
    }
    // Otherwise, let the buffer be destroyed
}

size_t CommandBufferManager::getActiveBufferCount() const {
    std::shared_lock lock(m_mutex);
    return m_activeBuffers.size();
}

size_t CommandBufferManager::getPooledBufferCount() const {
    std::shared_lock lock(m_mutex);

    size_t total = 0;
    for (const auto& [config, pool] : m_pooledBuffers) {
        total += pool.size();
    }
    return total;
}

void CommandBufferManager::cleanup() {
    std::unique_lock lock(m_mutex);

    // Clean up active buffers
    m_activeBuffers.clear();

    // Clean up pooled buffers
    for (auto& [config, pool] : m_pooledBuffers) {
        for (auto& buffer : pool) {
            if (buffer) {
                buffer->reset(); // Ensure clean state before destruction
            }
        }
        pool.clear();
    }
    m_pooledBuffers.clear();
}
