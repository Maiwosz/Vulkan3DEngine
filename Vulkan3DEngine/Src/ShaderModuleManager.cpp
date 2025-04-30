#include "ShaderModuleManager.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

ShaderModuleManager::ShaderModuleManager(
    const LogicalDevice& device,
    UniformBufferManager& uniformBufferManager,
    DescriptorLayoutManager& descriptorLayoutManager,
    PipelineLayoutManager& pipelineLayoutManager
) :
    m_device(device),
    m_uniformBufferManager(uniformBufferManager),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_pipelineLayoutManager(pipelineLayoutManager)
{
}

ShaderModuleHandle ShaderModuleManager::createFromSPIRV(
    const std::vector<uint32_t>& spirvCode,
    const ShaderReflection& reflection
) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    ShaderModuleHandle handle(m_nextModuleHandle++);
    m_modules[handle] = std::make_unique<ShaderModule>(m_device, createInfo, reflection);
    return handle;
}

void ShaderModuleManager::destroy(ShaderModuleHandle handle) {
    if (auto it = m_modules.find(handle); it != m_modules.end()) {
        m_modules.erase(it);
    }
}

ShaderModule& ShaderModuleManager::get(ShaderModuleHandle handle) {
    auto it = m_modules.find(handle);
    if (it == m_modules.end()) {
        throw std::runtime_error("Invalid shader module handle");
    }
    return *it->second;
}

bool ShaderModuleManager::isValid(ShaderModuleHandle handle) const {
    return m_modules.find(handle) != m_modules.end();
}

ShaderProgramHandle ShaderModuleManager::createProgram(
    const CombinedShader& shaders,
    const std::string& debugName
) {
    // Create a new program handle
    ShaderProgramHandle programHandle(m_nextProgramHandle++);
    ShaderResources resources;

    // Collect all bindings from all shader stages
    std::unordered_map<uint32_t, std::vector<DescriptorBindingInfo>> setBindings;
    std::vector<PushConstantRangeInfo> pushConstants;

    // Collect all shader stages
    VkShaderStageFlags combinedStageFlags = 0;

    // Process all shader modules to collect bindings and push constants
    for (const auto& [stage, moduleHandle] : shaders.stages) {
        // Get shader module
        const ShaderModule& module = *m_modules[moduleHandle];
        const ShaderReflection& reflection = module.getReflection();

        // Get shader stage flags
        VkShaderStageFlags stageFlags = 0;
        switch (stage) {
        case AssetLib::ShaderStage::Vertex:
            stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case AssetLib::ShaderStage::Fragment:
            stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case AssetLib::ShaderStage::Compute:
            stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
            // Add other stages as needed
        }

        combinedStageFlags |= stageFlags;

        // Process descriptor bindings
        for (const auto& binding : reflection.descriptorBindings) {
            // Group bindings by set
            uint32_t set = 0; // Default to set 0, modify if needed
            setBindings[set].push_back(binding);
        }

        // Add push constants
        for (const auto& pushConstant : reflection.pushConstants) {
            pushConstants.push_back(pushConstant);
        }
    }

    // Create descriptor layouts for each set
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    for (const auto& [set, bindings] : setBindings) {
        // Create descriptor layout for this set
        DescriptorLayoutHandle layoutHandle = createDescriptorLayout(bindings, combinedStageFlags);
        resources.descriptorLayouts[set] = layoutHandle;
        descriptorSetLayouts.push_back(m_descriptorLayoutManager.get(layoutHandle));

        // Create uniform buffers for UBO bindings
        for (const auto& binding : bindings) {
            if (binding.type == DescriptorBindingInfo::Type::UniformBuffer) {
                // Create uniform buffer with appropriate size and name
                std::string bufferName = binding.name;
                UniformBufferHandle bufferHandle = createUniformBuffer({ binding }, bufferName);
                resources.uniformBuffers[bufferName] = bufferHandle;
            }
        }
    }

    // Create pipeline layout
    resources.pipelineLayout = createPipelineLayout(descriptorSetLayouts, pushConstants);

    // Store resources for this program
    m_programResources[programHandle] = resources;

    return programHandle;
}

void ShaderModuleManager::destroyProgram(ShaderProgramHandle handle) {
    if (auto it = m_programResources.find(handle); it != m_programResources.end()) {
        auto& resources = it->second;

        // Clean up uniform buffers
        for (auto& [name, bufferHandle] : resources.uniformBuffers) {
            m_uniformBufferManager.freeBuffer(bufferHandle);
        }

        // Clean up descriptor layouts
        for (auto& [set, layoutHandle] : resources.descriptorLayouts) {
            m_descriptorLayoutManager.destroy(layoutHandle);
        }

        // Clean up pipeline layout
        m_pipelineLayoutManager.destroy(resources.pipelineLayout);

        // Remove resources from map
        m_programResources.erase(it);
    }
}

const ShaderResources& ShaderModuleManager::getProgramResources(ShaderProgramHandle handle) const {
    auto it = m_programResources.find(handle);
    if (it == m_programResources.end()) {
        throw std::runtime_error("Invalid shader program handle");
    }
    return it->second;
}

void ShaderModuleManager::updateUniformField(
    ShaderProgramHandle program,
    const std::string& bufferName,
    const std::string& fieldName,
    const UniformBufferManager::UniformValue& value
) {
    auto& resources = m_programResources.at(program);
    auto bufferIt = resources.uniformBuffers.find(bufferName);
    if (bufferIt == resources.uniformBuffers.end()) {
        throw std::runtime_error("Buffer not found: " + bufferName);
    }

    m_uniformBufferManager.updateField(bufferIt->second, fieldName, value);
}

void ShaderModuleManager::updateUniformArrayField(
    ShaderProgramHandle program,
    const std::string& bufferName,
    const std::string& fieldName,
    const std::vector<UniformBufferManager::UniformValue>& values
) {
    auto& resources = m_programResources.at(program);
    auto bufferIt = resources.uniformBuffers.find(bufferName);
    if (bufferIt == resources.uniformBuffers.end()) {
        throw std::runtime_error("Buffer not found: " + bufferName);
    }

    m_uniformBufferManager.updateFieldArray(bufferIt->second, fieldName, values);
}

void ShaderModuleManager::applyUniformUpdates(ShaderProgramHandle program) {
    auto& resources = m_programResources.at(program);
    for (auto& [name, bufferHandle] : resources.uniformBuffers) {
        m_uniformBufferManager.updateBuffer(bufferHandle);
    }
}

DescriptorLayoutHandle ShaderModuleManager::createDescriptorLayout(
    const std::vector<DescriptorBindingInfo>& bindings,
    VkShaderStageFlags shaderStages
) {
    DescriptorLayoutBuilder builder;

    for (const auto& binding : bindings) {
        VkDescriptorType descriptorType = convertDescriptorType(binding.type);
        builder.addBinding(binding.binding, descriptorType);
    }

    return m_descriptorLayoutManager.create(builder, shaderStages);
}

PipelineLayoutHandle ShaderModuleManager::createPipelineLayout(
    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
    const std::vector<PushConstantRangeInfo>& pushConstants
) {
    PipelineLayoutConfig config;
    config.descriptorSetLayouts = descriptorSetLayouts;

    // Convert push constants
    for (const auto& pushConstant : pushConstants) {
        VkPushConstantRange range = {};
        range.stageFlags = convertShaderStageFlags(pushConstant.stageFlags);
        range.offset = pushConstant.offset;
        range.size = pushConstant.size;
        config.pushConstantRanges.push_back(range);
    }

    return m_pipelineLayoutManager.createLayout(config);
}

UniformBufferHandle ShaderModuleManager::createUniformBuffer(
    const std::vector<DescriptorBindingInfo>& uniformBuffers,
    const std::string& bufferName
) {
    // Create fields for uniform buffer
    std::vector<UniformBufferManager::Field> fields;

    for (const auto& binding : uniformBuffers) {
        // Create a field for this binding
        UniformBufferManager::Field field;
        field.name = binding.name;

        // Determine the appropriate type based on binding size
        // This is a simplified approach; you might need a more sophisticated type inference
        switch (binding.size) {
        case 4:
            field.value = 0.0f; // Default float value
            break;
        case 8:
            field.value = glm::vec2(0.0f); // Default vec2 value
            break;
        case 12:
            field.value = glm::vec3(0.0f); // Default vec3 value
            break;
        case 16:
            field.value = glm::vec4(0.0f); // Default vec4 value
            break;
        case 64:
            field.value = glm::mat4(1.0f); // Default mat4 value
            break;
        default:
            field.value = 0.0f; // Default float as fallback
            break;
        }

        fields.push_back(field);
    }

    return m_uniformBufferManager.createBuffer(fields, bufferName);
}

VkShaderStageFlags ShaderModuleManager::convertShaderStageFlags(uint8_t flags) const {
    VkShaderStageFlags result = 0;

    // Example mapping - adjust according to your shader stage flags
    if (flags & 0x01) result |= VK_SHADER_STAGE_VERTEX_BIT;
    if (flags & 0x02) result |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (flags & 0x04) result |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (flags & 0x08) result |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (flags & 0x10) result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (flags & 0x20) result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    return result;
}

VkDescriptorType ShaderModuleManager::convertDescriptorType(DescriptorBindingInfo::Type type) const {
    switch (type) {
    case DescriptorBindingInfo::Type::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorBindingInfo::Type::CombinedImageSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorBindingInfo::Type::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorBindingInfo::Type::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorBindingInfo::Type::InputAttachment:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    default:
        throw std::runtime_error("Unknown descriptor type");
    }
}

std::vector<uint32_t> ShaderModuleManager::readFile(const std::string& filename) const {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    file.close();
    return buffer;
}