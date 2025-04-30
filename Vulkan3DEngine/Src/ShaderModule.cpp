#include "ShaderModule.h"
#include <stdexcept>



ShaderModule::ShaderModule(
    const LogicalDevice& device,
    const VkShaderModuleCreateInfo& createInfo,
    const ShaderReflection& reflection)
	: m_device(device),
	  m_reflection(reflection)
{
    if (vkCreateShaderModule(m_device.get(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}

ShaderModule::~ShaderModule() {
    vkDestroyShaderModule(m_device.get(), m_module, nullptr);
}