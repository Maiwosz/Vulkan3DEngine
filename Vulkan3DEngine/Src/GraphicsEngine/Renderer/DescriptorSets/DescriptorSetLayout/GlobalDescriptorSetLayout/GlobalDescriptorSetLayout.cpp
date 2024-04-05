#include "GlobalDescriptorSetLayout.h"
#include "../../../Renderer.h"

GlobalDescriptorSetLayout::GlobalDescriptorSetLayout(Renderer* renderer): DescriptorSetLayout(renderer)
{
	createBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	createLayout();
}

GlobalDescriptorSetLayout::~GlobalDescriptorSetLayout()
{
	
}