#include "ShaderModuleManager.h"
#include <stdexcept>
#include <VulkanHelper.h>

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

ShaderHandle ShaderModuleManager::createShader(
    const ShaderLib::ShaderMetadata& metadata,
    const std::vector<ShaderLib::CompiledStage>& stages
) {
    // Generowanie unikalnego uchwytu
    ShaderHandle handle(m_nextShaderHandle++);

    // Tworzenie modułów dla każdego etapu
    CombinedShader combinedShader;
    for (const auto& stage : stages) {
        // Utworzenie modułu shadera dla tego etapu
        ShaderModuleHandle moduleHandle = createModuleFromSPIRV(stage.spirv);

        // Zapisanie modułu w połączonym shaderze
        combinedShader.stages[stage.stage] = moduleHandle;
    }

    // Zapisanie połączonego shadera
    m_combinedShaders[handle] = std::move(combinedShader);

    // Tworzenie layoutów deskryptorów
    auto descriptorLayouts = createDescriptorLayouts(metadata);

    // Tworzenie layoutu potoku
    auto pipelineLayout = createPipelineLayout(metadata, descriptorLayouts);

    // Zapisanie zasobów shadera
    ShaderResources resources;
    resources.descriptorLayouts = std::move(descriptorLayouts);
    resources.pipelineLayout = pipelineLayout;
    resources.uniformBuffers = std::unordered_map<std::string, UniformBufferHandle>(); // Bufory będą tworzone na żądanie

    m_shaderResources[handle] = std::move(resources);

    // Zapisanie metadanych shadera
    m_shaderMetadata[handle] = metadata;

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
