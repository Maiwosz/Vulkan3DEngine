#include "DescriptorSetLayout.h"
#include "../../Renderer.h"

DescriptorSetLayout::DescriptorSetLayout(Renderer* renderer): m_renderer(renderer)
{
	/*createBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	createBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	createLayout();*/
}

DescriptorSetLayout::~DescriptorSetLayout()
{
	if (m_descriptorSetLayout) {
		vkDestroyDescriptorSetLayout(m_renderer->m_device->get(), m_descriptorSetLayout, nullptr);
	}
}

void DescriptorSetLayout::createBinding(uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorCount = descriptorCount;
	layoutBinding.descriptorType = descriptorType;
	layoutBinding.pImmutableSamplers = nullptr;
	layoutBinding.stageFlags = stageFlags;

	m_bindings.push_back(layoutBinding);
}

void DescriptorSetLayout::createLayout()
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
	layoutInfo.pBindings = m_bindings.data();

	if (vkCreateDescriptorSetLayout(m_renderer->m_device->get(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}
