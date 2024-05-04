#include "ModelDescriptorSet.h"
#include "Renderer.h"

ModelDescriptorSetLayout::ModelDescriptorSetLayout(Renderer* renderer) : DescriptorSetLayout(renderer)
{
    createBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    createLayout();
}

ModelDescriptorSetLayout::~ModelDescriptorSetLayout()
{

}

ModelDescriptorSet::ModelDescriptorSet(VkBuffer uniformBuffer, Renderer* renderer) :
    DescriptorSet(renderer->m_modelDescriptorSetLayout, renderer), m_renderer(renderer)
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

ModelDescriptorSet::~ModelDescriptorSet()
{
}

void ModelDescriptorSet::bind()
{
    m_renderer->m_currentDescriptorSets[1] = m_descriptorSet;
}
