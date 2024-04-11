#include "TransformDescriptorSet.h"
#include "../../../Renderer.h"
#include "../../DescriptorSetLayout/TransformDescriptorSetLayout/TransformDescriptorSetLayout.h"

TransformDescriptorSet::TransformDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer) :
    DescriptorSet(renderer->m_transformDescriptorSetLayout->get(), renderer), m_renderer(renderer)
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ModelUBO);

    VkWriteDescriptorSet write{};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    m_writes.push_back(write);

    vkUpdateDescriptorSets(m_renderer->m_device->get(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

TransformDescriptorSet::~TransformDescriptorSet()
{
}
