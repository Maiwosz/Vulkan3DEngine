#include "CommandPool.h"
#include "CommandBuffer.h"
#include <stdexcept>

CommandPool::CommandPool(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue)
	: m_device(device), m_queueFamilyIndex(queueFamilyIndex), m_queue(queue) 
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_pool));
}

CommandPool::~CommandPool() {
    vkDestroyCommandPool(m_device, m_pool, nullptr);
}

std::unique_ptr<CommandBuffer> CommandPool::beginSingleTimeCommands() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto cmd = std::make_unique<CommandBuffer>(*this);
    cmd->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return cmd;
}

void CommandPool::endSingleTimeCommands(std::unique_ptr<CommandBuffer> cmd) {
    std::lock_guard<std::mutex> lock(m_mutex);

    cmd->end();

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

    cmd->submit(m_queue, {}, {}, {}, fence);

    VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(m_device, fence, nullptr);
}