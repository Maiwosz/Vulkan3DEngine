#pragma once
#include <vulkan/vulkan.h>
#include <mutex>
#include <memory>
#include "QueueManager.h"

class CommandBuffer;

class CommandPool : public std::enable_shared_from_this<CommandPool> {
public:
    CommandPool(VkDevice device, uint32_t queueFamilyIndex,
        std::shared_ptr<QueueWrapper> queueWrapper, QueueType queueType);
    ~CommandPool();

    VkCommandPool get() const { return m_pool; }
    VkDevice getDevice() const { return m_device; }
    std::shared_ptr<QueueWrapper> getQueueWrapper() const { return m_queueWrapper; }
    uint32_t getQueueFamilyIndex() const { return m_queueFamilyIndex; }
    QueueType getQueueType() const { return m_queueType; }

    std::unique_ptr<CommandBuffer> beginSingleTimeCommands();
    void endSingleTimeCommands(std::unique_ptr<CommandBuffer> commandBuffer);

private:
    VkDevice m_device;
    VkCommandPool m_pool;
    std::shared_ptr<QueueWrapper> m_queueWrapper;
    uint32_t m_queueFamilyIndex;
    QueueType m_queueType;
    std::mutex m_poolMutex; // Mutex dla samego pool'a (alokacja command bufferów)
};
