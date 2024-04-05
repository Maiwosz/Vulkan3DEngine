#include "DescriptorPool.h"
#include "../../Renderer.h"

DescriptorPool::DescriptorPool(uint32_t objectCount, Renderer* renderer): m_renderer(renderer)
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(m_renderer->MAX_FRAMES_IN_FLIGHT);//Always number of frames in flight
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_renderer->MAX_FRAMES_IN_FLIGHT) * objectCount;//number of frames in flight times number of textures

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = (1 + objectCount * 2) * static_cast<uint32_t>(m_renderer->MAX_FRAMES_IN_FLIGHT);//1 for global descriptorset, 1 for mesh and 1 for texture descriptor set

    if (vkCreateDescriptorPool(m_renderer->m_device->get(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

DescriptorPool::~DescriptorPool()
{
    vkDestroyDescriptorPool(m_renderer->m_device->get(), m_descriptorPool, nullptr);
}

bool DescriptorPool::allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    if (vkAllocateDescriptorSets(m_renderer->m_device->get(), &allocInfo, &descriptor) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors)
{
    vkFreeDescriptorSets(m_renderer->m_device->get(), m_descriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
}

void DescriptorPool::resetPool()
{
    vkResetDescriptorPool(m_renderer->m_device->get(), m_descriptorPool, 0);
}
