#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <json.hpp>


struct DescriptorBindingInfo {
    uint32_t binding;
    enum class Type {
        UniformBuffer,
        CombinedImageSampler,
        StorageBuffer,
        StorageImage,
        InputAttachment
    } type;
    uint8_t stageFlags; // Bit flags dla ShaderStageFlags
    uint32_t size;
    std::string name;
};

struct PushConstantRangeInfo {
    uint8_t stageFlags; // Bit flags dla ShaderStageFlags
    uint32_t offset;
    uint32_t size;
};

struct ShaderReflection {
    std::vector<DescriptorBindingInfo> descriptorBindings;
    std::vector<PushConstantRangeInfo> pushConstants;
    std::string entryPoint;
    uint32_t spirvVersion;
};

