#pragma once
#include "VulkanContext.h"
#include "CommandBuffer.h"
#include <unordered_map>
#include <vector>
#include <mutex>

class CommandBufferManager {
public:
    struct Configuration {
        LogicalDevice::QueueType queueType; // enum: Graphics, Transfer, Compute
        VkCommandBufferLevel level;         // Primary/Secondary
        VkCommandBufferUsageFlags usageFlags;

        bool operator==(const Configuration& other) const;
    };

    explicit CommandBufferManager(VulkanContext& context);

    std::unique_ptr<CommandBuffer> acquireBuffer(const Configuration& config);
    void releaseBuffer(std::unique_ptr<CommandBuffer> buffer);

private:
    struct ConfigurationHash {
        size_t operator()(const Configuration& config) const;
    };

    VulkanContext& m_context;
    std::unordered_map<Configuration, std::vector<std::unique_ptr<CommandBuffer>>, ConfigurationHash> m_availableBuffers;
    std::mutex m_mutex;
};