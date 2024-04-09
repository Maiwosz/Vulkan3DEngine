#include "TransformDescriptorSetLayout.h"
#include "../../../Renderer.h"

TransformDescriptorSetLayout::TransformDescriptorSetLayout(Renderer* renderer): DescriptorSetLayout(renderer)
{
	createBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	createLayout();
}

TransformDescriptorSetLayout::~TransformDescriptorSetLayout()
{
	
}