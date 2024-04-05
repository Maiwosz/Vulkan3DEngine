#include "TextureDescriptorSetLayout.h"
#include "../../../Renderer.h"

TextureDescriptorSetLayout::TextureDescriptorSetLayout(Renderer* renderer): DescriptorSetLayout(renderer)
{
	createBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	createLayout();
}

TextureDescriptorSetLayout::~TextureDescriptorSetLayout()
{
	
}