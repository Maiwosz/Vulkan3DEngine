#include "DescriptorSet.h"
#include "../../Renderer.h"
#include "../DescriptorPool/DescriptorPool.h"

DescriptorSet::DescriptorSet(const VkDescriptorSetLayout descriptorSetLayout, Renderer* renderer): m_renderer(renderer)
{
    m_renderer->m_descriptorPool->allocateDescriptor(descriptorSetLayout, m_descriptorSet);
    
}

DescriptorSet::~DescriptorSet()
{
}

void DescriptorSet::update()
{
    vkUpdateDescriptorSets(m_renderer->m_device->get(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

void DescriptorSet::bind()
{
    m_renderer->m_currentDescriptorSets.push_back(m_descriptorSet);
}
