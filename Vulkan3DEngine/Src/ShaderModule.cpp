#include "ShaderModule.h"
#include <stdexcept>

ShaderModule::ShaderModule(
    const LogicalDevice& device,
    const VkShaderModuleCreateInfo& createInfo
)
	: m_device(device)
{
    if (vkCreateShaderModule(m_device.get(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}

ShaderModule::ShaderModule(
    const LogicalDevice& device,
    const std::vector<uint32_t>& spirvCode
)
    : m_device(device)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    if (vkCreateShaderModule(m_device.get(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}

ShaderModule::~ShaderModule() {
    vkDestroyShaderModule(m_device.get(), m_module, nullptr);
}