#include "ShaderModuleManager.h"
#include <stdexcept>
#include <VulkanHelper.h>
#include <spdlog/spdlog.h>
#include <TypeConversions.h>
#include <fstream>

ShaderModuleManager::ShaderModuleManager(
    const LogicalDevice& device,
    UniformBufferManager& uniformBufferManager,
    DescriptorLayoutManager& descriptorLayoutManager,
    PipelineLayoutManager& pipelineLayoutManager
) : m_device(device),
m_uniformBufferManager(uniformBufferManager),
m_descriptorLayoutManager(descriptorLayoutManager),
m_pipelineLayoutManager(pipelineLayoutManager) {
}

ShaderModuleManager::~ShaderModuleManager() {
    // Zniszczenie wszystkich modułów
    for (auto& [handle, module] : m_modules) {
        // Moduły są czyszczone przez unique_ptr
    }

    // Zniszczenie shaderów - nie ma potrzeby ręcznego zarządzania pamięcią
}

ShaderModuleHandle ShaderModuleManager::createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode) {
    // Tworzenie modułu shadera
    auto module = std::make_unique<ShaderModule>(m_device, spirvCode);

    // Generowanie unikalnego uchwytu
    ShaderModuleHandle handle(m_nextModuleHandle++);

    // Zapisanie modułu
    m_modules[handle] = std::move(module);

    return handle;
}

ShaderModuleHandle ShaderModuleManager::createModuleFromSPIRVFile(const std::string& filePath) {
    SPDLOG_INFO("Loading SPIRV shader from file: {}", filePath);

    // Open the file and read as binary
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        SPDLOG_ERROR("Failed to open SPIRV file: {}", filePath);
        throw std::runtime_error("Failed to open SPIRV file: " + filePath);
    }

    // Get file size and allocate buffer
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % sizeof(uint32_t) != 0) {
        SPDLOG_ERROR("SPIRV file size is not a multiple of 4 bytes: {}", filePath);
        throw std::runtime_error("Invalid SPIRV file format: " + filePath);
    }

    // Calculate how many uint32_t elements we need
    size_t codeSize = fileSize / sizeof(uint32_t);
    std::vector<uint32_t> spirvCode(codeSize);

    // Read the file from the beginning
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);
    file.close();

    // Validate SPIRV magic number (SPIR-V binaries start with magic number 0x07230203)
    if (spirvCode.empty() || spirvCode[0] != 0x07230203) {
        SPDLOG_ERROR("Invalid SPIRV file format (missing magic number): {}", filePath);
        throw std::runtime_error("Invalid SPIRV file format: " + filePath);
    }

    SPDLOG_INFO("Successfully loaded SPIRV file: {}, size: {} bytes", filePath, fileSize);

    // Create shader module using existing function
    return createModuleFromSPIRV(spirvCode);
}

ShaderHandle ShaderModuleManager::createShader(
    const ShaderLib::ShaderMetadata& metadata,
    const std::vector<ShaderLib::CompiledStage>& stages
) {
    SPDLOG_INFO("Creating new shader with {} available stages", metadata.availableStages);

    // Detailed logging of shader metadata
    SPDLOG_INFO("Shader metadata:");
    SPDLOG_INFO("  - Uses GlobalUBO: {}", metadata.usesGlobalUBO ? "yes" : "no");
    SPDLOG_INFO("  - Uses ObjectUBO: {}", metadata.usesObjectUBO ? "yes" : "no");

    // Logging push constant information
    if (!metadata.pushConstants.empty()) {
        SPDLOG_INFO("  - Push constants ({}): ", metadata.pushConstants.size());
        for (const auto& pc : metadata.pushConstants) {
            SPDLOG_INFO("    * Offset: {}, Size: {}, Stages: {}",
                pc.offset, pc.size, pc.stages);
        }
    }
    else {
        SPDLOG_INFO("  - No push constants");
    }

    // Logging descriptor information
    if (!metadata.descriptors.empty()) {
        SPDLOG_INFO("  - Descriptors ({}): ", metadata.descriptors.size());
        for (const auto& desc : metadata.descriptors) {
            SPDLOG_INFO("    * Set: {}, Binding: {}, Type: {}, Name: '{}'",
                desc.set, desc.binding, static_cast<int>(desc.type), desc.name);
        }
    }
    else {
        SPDLOG_INFO("  - No descriptors");
    }

    // Logging custom UBO information
    if (!metadata.customUBOs.empty()) {
        SPDLOG_INFO("  - Custom UBOs ({}): ", metadata.customUBOs.size());
        for (const auto& ubo : metadata.customUBOs) {
            SPDLOG_INFO("    * Name: '{}', Set: {}, Binding: {}, Size: {} bytes",
                ubo.name, ubo.set, ubo.binding, ubo.size);
            if (!ubo.variables.empty()) {
                SPDLOG_DEBUG("      Variables in UBO '{}':", ubo.name);
                for (const auto& var : ubo.variables) {
                    SPDLOG_DEBUG("      - '{}': type {}, offset {}, size {}",
                        var.name, ShaderLib::TypeConversion::UniformTypeToString(var.type), var.offset, var.size);
                }
            }
        }
    }
    else {
        SPDLOG_INFO("  - No custom UBOs");
    }

    // Logging GlobalUBO information if used
    if (metadata.usesGlobalUBO) {
        SPDLOG_INFO("  - GlobalUBO: Set {}, Binding {}, Size {} bytes, {} variables",
            metadata.globalUBO.set, metadata.globalUBO.binding, metadata.globalUBO.size, metadata.globalUBO.variables.size());
    }

    // Logging ObjectUBO information if used
    if (metadata.usesObjectUBO) {
        SPDLOG_INFO("  - ObjectUBO: Set {}, Binding {}, Size {} bytes, {} variables",
            metadata.objectUBO.set, metadata.objectUBO.binding, metadata.objectUBO.size, metadata.objectUBO.variables.size());
    }

    // Logging shader stage information
    SPDLOG_INFO("Shader stages ({}):", stages.size());
    for (const auto& stage : stages) {
        SPDLOG_INFO("  - Stage: {}, SPIRV Size: {} bytes",
            ShaderLib::TypeConversion::StageToString(stage.stage), stage.spirv.size() * sizeof(uint32_t));
    }

    // Generating unique handle
    ShaderHandle handle(m_nextShaderHandle++);
    SPDLOG_INFO("Generated shader handle: {}", handle.id);

    // Creating modules for each stage
    CombinedShader combinedShader;
    for (const auto& stage : stages) {
        // Creating shader module for this stage
        SPDLOG_DEBUG("Creating shader module for stage {}", ShaderLib::TypeConversion::StageToString(stage.stage));
        ShaderModuleHandle moduleHandle = createModuleFromSPIRV(stage.spirv);
        // Storing module in the combined shader
        combinedShader.stages[stage.stage] = moduleHandle;
        SPDLOG_DEBUG("Created shader module: {}", moduleHandle.id);
    }

    // Storing the combined shader
    m_combinedShaders[handle] = std::move(combinedShader);
    SPDLOG_DEBUG("Stored combined shader in map (shader count: {})", m_combinedShaders.size());

    // Creating descriptor layouts
    SPDLOG_DEBUG("Creating descriptor layouts");
    auto descriptorLayouts = createDescriptorLayouts(metadata);

    // Creating pipeline layout
    SPDLOG_DEBUG("Creating pipeline layout");
    auto pipelineLayout = createPipelineLayout(metadata, descriptorLayouts);

    // Storing shader resources
    ShaderResources resources;
    resources.descriptorLayouts = std::move(descriptorLayouts);
    resources.pipelineLayout = pipelineLayout;
    resources.uniformBuffers = std::unordered_map<std::string, UniformBufferHandle>(); // Buffers will be created on demand
    m_shaderResources[handle] = std::move(resources);
    SPDLOG_DEBUG("Stored shader resources in map (resource count: {})", m_shaderResources.size());

    // Storing shader metadata
    m_shaderMetadata[handle] = metadata;
    SPDLOG_INFO("Shader created successfully (handle: {})", handle.id);
    return handle;
}

void ShaderModuleManager::destroyModule(ShaderModuleHandle handle) {
    if (isModuleValid(handle)) {
        m_modules.erase(handle);
    }
}

void ShaderModuleManager::destroyShader(ShaderHandle handle) {
    if (isShaderValid(handle)) {
        // Usunięcie wszystkich modułów shadera
        auto& combinedShader = m_combinedShaders[handle];
        for (auto& [stage, moduleHandle] : combinedShader.stages) {
            destroyModule(moduleHandle);
        }

        // Usunięcie połączonego shadera
        m_combinedShaders.erase(handle);

        // Usunięcie zasobów shadera
        m_shaderResources.erase(handle);

        // Usunięcie metadanych shadera
        m_shaderMetadata.erase(handle);
    }
}

ShaderModule& ShaderModuleManager::getModule(ShaderModuleHandle handle) {
    if (!isModuleValid(handle)) {
        throw std::runtime_error("Invalid shader module handle");
    }

    return *m_modules[handle];
}

ShaderModule* ShaderModuleManager::getModuleForStage(ShaderHandle shader, ShaderLib::Stage stage) {
    if (!isShaderValid(shader)) {
        return nullptr;
    }

    auto& combinedShader = m_combinedShaders[shader];
    auto it = combinedShader.stages.find(stage);
    if (it != combinedShader.stages.end()) {
        return &getModule(it->second);
    }

    return nullptr;
}

const CombinedShader& ShaderModuleManager::getCombinedShader(ShaderHandle handle) const {
    auto it = m_combinedShaders.find(handle);
    if (it == m_combinedShaders.end()) {
        throw std::runtime_error("Invalid shader handle");
    }

    return it->second;
}

const ShaderResources& ShaderModuleManager::getShaderResources(ShaderHandle handle) const {
    auto it = m_shaderResources.find(handle);
    if (it == m_shaderResources.end()) {
        throw std::runtime_error("Invalid shader handle");
    }

    return it->second;
}

const ShaderLib::ShaderMetadata& ShaderModuleManager::getShaderMetadata(ShaderHandle handle) const {
    auto it = m_shaderMetadata.find(handle);
    if (it == m_shaderMetadata.end()) {
        throw std::runtime_error("Invalid shader handle");
    }

    return it->second;
}

bool ShaderModuleManager::isModuleValid(ShaderModuleHandle handle) const {
    return m_modules.find(handle) != m_modules.end();
}

bool ShaderModuleManager::isShaderValid(ShaderHandle handle) const {
    return m_combinedShaders.find(handle) != m_combinedShaders.end();
}

std::unordered_map<uint32_t, DescriptorLayoutHandle> ShaderModuleManager::createDescriptorLayouts(
    const ShaderLib::ShaderMetadata& metadata
) {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> result;

    // Grupowanie deskryptorów według setu
    std::unordered_map<uint32_t, std::vector<ShaderLib::DescriptorBinding>> descriptorsBySet;

    for (const auto& descriptor : metadata.descriptors) {
        descriptorsBySet[descriptor.set].push_back(descriptor);
    }

    // Tworzenie layoutów deskryptorów dla każdego setu
    for (const auto& [set, descriptors] : descriptorsBySet) {
        // Tworzenie buildera layoutu deskryptora
        DescriptorLayoutBuilder builder;

        // Dodawanie bindingów do buildera
        for (const auto& descriptor : descriptors) {
            // Konwersja typów z ShaderLib na typy Vulkan
            VkDescriptorType vulkanDescriptorType =
                static_cast<VkDescriptorType>(ShaderLib::GetVulkanDescriptorType(descriptor.type));

            // Dodanie bindingu do buildera
            builder.addBinding(descriptor.binding, vulkanDescriptorType);
        }

        // Uzyskanie flag etapu shadera dla tego setu
        VkShaderStageFlags stageFlags = 0;
        for (const auto& descriptor : descriptors) {
            stageFlags |= ShaderLib::GetVulkanShaderStageFlags(descriptor.stages);
        }

        // Tworzenie layoutu deskryptora za pomocą metody create (zamiast createLayout)
        DescriptorLayoutHandle layoutHandle = m_descriptorLayoutManager.create(
            builder,
            stageFlags
        );

        // Zapisanie layoutu deskryptora
        result[set] = layoutHandle;
    }

    return result;
}

PipelineLayoutHandle ShaderModuleManager::createPipelineLayout(
    const ShaderLib::ShaderMetadata& metadata,
    const std::unordered_map<uint32_t, DescriptorLayoutHandle>& descriptorLayouts
) {
    // Konwersja push constants do formatu odpowiedniego dla PipelineLayoutManager
    std::vector<ShaderLib::VulkanPushConstantRange> pushConstantRanges;

    for (const auto& pushConstant : metadata.pushConstants) {
        ShaderLib::VulkanPushConstantRange range;
        range.stageFlags = ShaderLib::GetVulkanShaderStageFlags(pushConstant.stages);
        range.offset = pushConstant.offset;
        range.size = pushConstant.size;

        pushConstantRanges.push_back(range);
    }

    // Zbieranie layoutów deskryptorów
    std::vector<VkDescriptorSetLayout> vkLayouts;

    // Dodanie ich w kolejności od najniższego setu do najwyższego
    for (uint32_t i = 0; i <= ShaderLib::CUSTOM_DESCRIPTOR_SET; ++i) {
        auto it = descriptorLayouts.find(i);
        if (it != descriptorLayouts.end()) {
            // Pobierz VkDescriptorSetLayout z DescriptorLayoutManager używając naszego uchwytu
            VkDescriptorSetLayout vkLayout = m_descriptorLayoutManager.get(it->second);
            vkLayouts.push_back(vkLayout);
        }
    }

    // Tworzenie konfiguracji layoutu potoku
    PipelineLayoutConfig layoutConfig;

    // Konwersja VulkanPushConstantRange do VkPushConstantRange
    layoutConfig.pushConstantRanges.reserve(pushConstantRanges.size());
    for (const auto& range : pushConstantRanges) {
        VkPushConstantRange vkRange;
        vkRange.stageFlags = range.stageFlags;
        vkRange.offset = range.offset;
        vkRange.size = range.size;
        layoutConfig.pushConstantRanges.push_back(vkRange);
    }

    layoutConfig.descriptorSetLayouts = std::move(vkLayouts);

    // Tworzenie layoutu potoku z konfiguracją
    return m_pipelineLayoutManager.createLayout(layoutConfig);
}

std::unordered_map<std::string, UniformBufferHandle> ShaderModuleManager::createUniformBuffers(
    const ShaderLib::ShaderMetadata& metadata
) {
    std::unordered_map<std::string, UniformBufferHandle> result;

    // Tworzenie buforów uniform nie jest domyślnie wykonywane - będą tworzone na żądanie

    return result;
}

// Nowe metody do obsługi buforów uniform

UniformBufferHandle ShaderModuleManager::createGlobalUniformBuffer(ShaderHandle shaderHandle) {
    if (!isShaderValid(shaderHandle)) {
        return UniformBufferHandle(0);
    }

    const auto& metadata = m_shaderMetadata[shaderHandle];
    if (!metadata.usesGlobalUBO) {
        return UniformBufferHandle(0);
    }

    // Utwórz bufor
    UniformBufferHandle bufferHandle = m_uniformBufferManager.createGlobalBuffer(metadata);

    // Zapisz uchwyt w zasobach shadera
    if (bufferHandle) {
        m_shaderResources[shaderHandle].uniformBuffers["global"] = bufferHandle;
    }

    return bufferHandle;
}

UniformBufferHandle ShaderModuleManager::createObjectUniformBuffer(ShaderHandle shaderHandle) {
    if (!isShaderValid(shaderHandle)) {
        return UniformBufferHandle(0);
    }

    const auto& metadata = m_shaderMetadata[shaderHandle];
    if (!metadata.usesObjectUBO) {
        return UniformBufferHandle(0);
    }

    // Utwórz bufor
    UniformBufferHandle bufferHandle = m_uniformBufferManager.createObjectBuffer(metadata);

    // Zapisz uchwyt w zasobach shadera
    if (bufferHandle) {
        m_shaderResources[shaderHandle].uniformBuffers["object"] = bufferHandle;
    }

    return bufferHandle;
}

UniformBufferHandle ShaderModuleManager::createCustomUniformBuffer(ShaderHandle shaderHandle, const std::string& name) {
    if (!isShaderValid(shaderHandle)) {
        return UniformBufferHandle(0);
    }

    const auto& metadata = m_shaderMetadata[shaderHandle];



    // Sprawdź czy istnieje UBO o podanej nazwie
    bool exists = false;
    for (const auto& ubo : metadata.customUBOs) {
        if (ubo.name == name) {
            exists = true;
            break;
        }
    }

    if (!exists) {
        return UniformBufferHandle(0);
    }

    // Utwórz bufor
    UniformBufferHandle bufferHandle = m_uniformBufferManager.createCustomBuffer(metadata, name);

    // Zapisz uchwyt w zasobach shadera
    if (bufferHandle) {
        m_shaderResources[shaderHandle].uniformBuffers[name] = bufferHandle;
    }

    return bufferHandle;
}

UniformBufferHandle ShaderModuleManager::acquireUniformBuffer(ShaderHandle shaderHandle, const std::string& name) {
    if (!isShaderValid(shaderHandle)) {
        return UniformBufferHandle(0);
    }

    const auto& metadata = m_shaderMetadata[shaderHandle];

    // Sprawdź jakiego rodzaju bufor jest potrzebny
    if (name == "global" && metadata.usesGlobalUBO) {
        return m_uniformBufferManager.acquireBuffer(metadata.globalUBO);
    }
    else if (name == "object" && metadata.usesObjectUBO) {
        return m_uniformBufferManager.acquireBuffer(metadata.objectUBO);
    }
    else {
        // Sprawdź czy istnieje custom UBO o podanej nazwie
        for (const auto& ubo : metadata.customUBOs) {
            if (ubo.name == name) {
                return m_uniformBufferManager.acquireBuffer(ubo);
            }
        }
    }

    return UniformBufferHandle(0);
}

void ShaderModuleManager::releaseUniformBuffer(UniformBufferHandle handle) {
    m_uniformBufferManager.releaseBuffer(handle);
}

void ShaderModuleManager::updateUniformBuffer(
    UniformBufferHandle handle,
    const void* data,
    uint32_t size,
    uint32_t offset
) {
    m_uniformBufferManager.updateBuffer(handle, data, size, offset);
}
