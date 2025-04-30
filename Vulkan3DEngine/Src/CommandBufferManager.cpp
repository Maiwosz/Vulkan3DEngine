#include "CommandBufferManager.h"
#include <functional>

bool CommandBufferManager::Configuration::operator==(const Configuration& other) const {
    return queueType == other.queueType &&
        level == other.level &&
        usageFlags == other.usageFlags;
}

size_t CommandBufferManager::ConfigurationHash::operator()(const Configuration& config) const {
    size_t seed = 0;
    std::hash<int> intHash;
    std::hash<VkCommandBufferLevel> levelHash;
    std::hash<VkCommandBufferUsageFlags> flagsHash;

    seed ^= intHash(static_cast<int>(config.queueType)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= levelHash(config.level) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= flagsHash(config.usageFlags) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

CommandBufferManager::CommandBufferManager(VulkanContext& context)
    : m_context(context) {
}

std::unique_ptr<CommandBuffer> CommandBufferManager::acquireBuffer(const Configuration& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& buffers = m_availableBuffers[config];
    if (!buffers.empty()) {
        auto buffer = std::move(buffers.back());
        buffers.pop_back();
        buffer->reset(); // Reset before reuse
        return buffer;
    }

    // Create new buffer
    CommandPool* pool = nullptr;
    switch (config.queueType) {
    case LogicalDevice::QueueType::Graphics:
        pool = &m_context.graphicsCommandPool();
        break;
    case LogicalDevice::QueueType::Transfer:
        pool = &m_context.transferCommandPool();
        break;
    case LogicalDevice::QueueType::Compute:
        pool = &m_context.computeCommandPool();
        break;
    }

    return std::make_unique<CommandBuffer>(*pool, config.level);
}

void CommandBufferManager::releaseBuffer(std::unique_ptr<CommandBuffer> buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Configuration config{
        .queueType = static_cast<LogicalDevice::QueueType>(buffer->getQueueFamilyIndex()),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .usageFlags = 0
    };

    buffer->reset(); // Optional: Reset before storing
    m_availableBuffers[config].emplace_back(std::move(buffer));
}