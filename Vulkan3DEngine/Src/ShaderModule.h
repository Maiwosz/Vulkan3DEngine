#pragma once
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"

class ShaderModule {
public:
    ShaderModule(
        const LogicalDevice& device,
        const VkShaderModuleCreateInfo& createInfo
    );

    ShaderModule(
        const LogicalDevice& device,
        const std::vector<uint32_t>& spirvCode
    );

    ~ShaderModule();

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    VkShaderModule get() const { return m_module; }

private:
    const LogicalDevice& m_device;
    VkShaderModule m_module;
};