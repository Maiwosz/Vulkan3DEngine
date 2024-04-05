#include "TextureDescriptorSet.h"
#include "../../../Renderer.h"
#include "../../DescriptorSetLayout/TextureDescriptorSetLayout/TextureDescriptorSetLayout.h"

TextureDescriptorSet::TextureDescriptorSet(VkImageView imageView, VkSampler sampler, Renderer* renderer):
	DescriptorSet(renderer->m_textureDescriptorSetLayout->get(), renderer), m_renderer(renderer)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    m_writes.push_back(write);

    vkUpdateDescriptorSets(m_renderer->m_device->get(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

TextureDescriptorSet::~TextureDescriptorSet()
{
}
