#pragma once
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"
#include "ShaderReflection.h"

class ShaderModule {
public:
    ShaderModule(
        const LogicalDevice& device,
        const VkShaderModuleCreateInfo& createInfo,
        const ShaderReflection& reflection
    );
    ~ShaderModule();

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    VkShaderModule get() const { return m_module; }
    const ShaderReflection& getReflection() const { return m_reflection; }

private:
    const LogicalDevice& m_device;
    VkShaderModule m_module;
    ShaderReflection m_reflection;
};