#pragma once
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

class DescriptorLayoutBuilder {
public:
    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

    const std::vector<VkDescriptorSetLayoutBinding>& getBindings() const { return bindings; }
private:
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};