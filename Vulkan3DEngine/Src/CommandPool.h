#pragma once
#include <vulkan/vulkan.h>
#include <mutex>
#include <memory>

class CommandBuffer;

class CommandPool {
public:
    CommandPool(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue);
    ~CommandPool();

    VkCommandPool get() const { return m_pool; }
    VkDevice getDevice() const { return m_device; }
    VkQueue getQueue() const { return m_queue; } 
    uint32_t getQueueFamilyIndex() const { return m_queueFamilyIndex; }

    std::unique_ptr<CommandBuffer> beginSingleTimeCommands();
    void endSingleTimeCommands(std::unique_ptr<CommandBuffer> commandBuffer);

private:
    VkDevice m_device;
    VkCommandPool m_pool;
    VkQueue m_queue;
    uint32_t m_queueFamilyIndex;
    std::mutex m_mutex;
};