#include "ShaderManager.h"
#include "AssetManager.h"
#include <stdexcept>
#include <VulkanHelper.h>
#include <spdlog/spdlog.h>
#include <TypeConversions.h>

ShaderManager::ShaderManager(
    const LogicalDevice& device,
    ShaderModuleManager& shaderModuleManager,
    DescriptorLayoutManager& descriptorLayoutManager,
    PipelineLayoutManager& pipelineLayoutManager
) : m_device(device),
m_shaderModuleManager(shaderModuleManager),
m_descriptorLayoutManager(descriptorLayoutManager),
m_pipelineLayoutManager(pipelineLayoutManager) {
}

ShaderManager::~ShaderManager() {
    // Clean up all shader assets
    for (auto& [filename, asset] : m_shaderAssets) {
        destroyShader(asset.handle);
    }
    m_shaderAssets.clear();
}

bool ShaderManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    if (handle.type != AssetType::Shader) {
        SPDLOG_ERROR("ShaderManager received non-shader asset: {}", handle.filename);
        return false;
    }

    // Check if already prepared
    if (isAssetReady(handle.filename)) {
        SPDLOG_DEBUG("Shader asset already prepared: {}", handle.filename);
        return true;
    }

    try {
        // Read shader data from asset
        auto [metadata, stages] = AssetLib::ReadShader(data);

        SPDLOG_INFO("Preparing shader asset: {}", handle.filename);
        SPDLOG_DEBUG("Shader has {} stages", stages.size());

        // Create the shader and all associated resources
        ShaderHandle shaderHandle = createShader(metadata, stages);

        // Find the newly created shader asset
        ShaderAsset* assetPtr = nullptr;
        for (auto& [_, asset] : m_shaderAssets) {
            if (asset.handle == shaderHandle) {
                assetPtr = &asset;
                break;
            }
        }

        if (!assetPtr) {
            throw std::runtime_error("Failed to find newly created shader asset");
        }

        // Store the asset with the proper filename
        ShaderAsset asset = std::move(*assetPtr);

        // Remove the temporary entry
        for (auto it = m_shaderAssets.begin(); it != m_shaderAssets.end(); ++it) {
            if (it->second.handle == shaderHandle) {
                m_shaderAssets.erase(it);
                break;
            }
        }

        // Store with the correct filename
        m_shaderAssets[handle.filename] = std::move(asset);

        SPDLOG_INFO("Successfully prepared shader asset: {} (memory: {} bytes)",
            handle.filename, m_shaderAssets[handle.filename].memorySize);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to prepare shader asset {}: {}", handle.filename, e.what());
        return false;
    }
}

void ShaderManager::unloadAsset(const std::string& filename) {
    auto it = m_shaderAssets.find(filename);
    if (it != m_shaderAssets.end()) {
        SPDLOG_INFO("Unloading shader asset: {}", filename);

        // Destroy the shader and its modules
        destroyShader(it->second.handle);

        // Remove from cache
        m_shaderAssets.erase(it);

        SPDLOG_DEBUG("Shader asset unloaded: {}", filename);
    }
}

bool ShaderManager::isAssetReady(const std::string& filename) const {
    return m_shaderAssets.find(filename) != m_shaderAssets.end();
}

uint64_t ShaderManager::getAssetSize(const std::string& filename) const {
    auto it = m_shaderAssets.find(filename);
    if (it != m_shaderAssets.end()) {
        return it->second.memorySize;
    }
    return 0;
}

std::vector<AssetDependency> ShaderManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    // Shaders typically don't have dependencies on other assets
    return {};
}

std::any ShaderManager::getResourceInternal(const AssetHandle& handle) const {
    auto it = m_shaderAssets.find(handle.filename);
    if (it != m_shaderAssets.end()) {
        return it->second.handle;
    }

    SPDLOG_WARN("Shader resource not found: {}", handle.filename);
    return ShaderHandle{};
}

std::any ShaderManager::getHandleInternal(const std::string& filename) const {
    auto it = m_shaderAssets.find(filename);
    if (it != m_shaderAssets.end()) {
        return it->second.handle;
    }

    SPDLOG_WARN("Shader handle not found: {}", filename);
    return ShaderHandle{};
}

const ShaderLib::ShaderMetadata& ShaderManager::getShaderMetadata(ShaderHandle handle) const {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            return asset.metadata;
        }
    }
    throw std::runtime_error("Invalid shader handle");
}

const ShaderResources& ShaderManager::getShaderResources(ShaderHandle handle) const {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            return asset.resources;
        }
    }
    throw std::runtime_error("Invalid shader handle");
}

ShaderModule* ShaderManager::getModuleForStage(ShaderHandle shader, ShaderLib::Stage stage) {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == shader) {
            auto it = asset.combinedShader.stages.find(stage);
            if (it != asset.combinedShader.stages.end()) {
                return m_shaderModuleManager.getModule(it->second);
            }
            return nullptr;
        }
    }
    return nullptr;
}

const CombinedShader& ShaderManager::getCombinedShader(ShaderHandle handle) const {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            return asset.combinedShader;
        }
    }
    throw std::runtime_error("Invalid shader handle");
}

// Private implementation methods

ShaderHandle ShaderManager::createShader(
    const ShaderLib::ShaderMetadata& metadata,
    const std::vector<ShaderLib::CompiledStage>& stages
) {
    SPDLOG_INFO("Creating new shader with {} available stages", metadata.availableStages);

    // Log shader metadata
    SPDLOG_DEBUG("Shader metadata:");
    SPDLOG_DEBUG("  - Uses GlobalUBO: {}", metadata.usesGlobalUBO ? "yes" : "no");
    SPDLOG_DEBUG("  - Uses ObjectUBO: {}", metadata.usesObjectUBO ? "yes" : "no");
    SPDLOG_DEBUG("  - Descriptors: {}", metadata.descriptors.size());
    SPDLOG_DEBUG("  - Push constants: {}", metadata.pushConstants.size());

    // Generate unique handle
    ShaderHandle handle(m_nextShaderHandle++);

    // Create modules for each stage
    CombinedShader combinedShader;
    ShaderStageConfig stageConfig;

    for (const auto& stage : stages) {
        SPDLOG_DEBUG("Creating shader module for stage {}",
            ShaderLib::TypeConversion::StageToString(stage.stage));

        ShaderModuleHandle moduleHandle = m_shaderModuleManager.createModuleFromSPIRV(stage.spirv);
        if (!moduleHandle) {
            // Clean up already created modules
            for (const auto& [existingStage, existingHandle] : combinedShader.stages) {
                m_shaderModuleManager.destroyModule(existingHandle);
            }
            throw std::runtime_error("Failed to create shader module for stage");
        }

        combinedShader.stages[stage.stage] = moduleHandle;

        switch (stage.stage) {
        case ShaderLib::Stage::Vertex:
            stageConfig.vertexShader = moduleHandle;
            break;
        case ShaderLib::Stage::Fragment:
            stageConfig.fragmentShader = moduleHandle;
            break;
        case ShaderLib::Stage::Geometry:
            stageConfig.geometryShader = moduleHandle;
            break;
        case ShaderLib::Stage::TessellationControl:
            stageConfig.tessControlShader = moduleHandle;
            break;
        case ShaderLib::Stage::TessellationEvaluation:
            stageConfig.tessEvalShader = moduleHandle;
            break;
        case ShaderLib::Stage::Compute:
            stageConfig.computeShader = moduleHandle;
            break;
        }
    }

    // Create descriptor layouts
    auto descriptorLayouts = createDescriptorLayouts(metadata);

    // Create pipeline layout
    PipelineLayoutHandle pipelineLayout = createPipelineLayout(
        metadata,
        descriptorLayouts
    );

    // Create shader asset and store it
    ShaderAsset asset;
    asset.handle = handle;
    asset.combinedShader = std::move(combinedShader);
    asset.resources.descriptorLayouts = std::move(descriptorLayouts);
    asset.resources.pipelineLayout = pipelineLayout;
    asset.resources.stageConfig = std::move(stageConfig);
    asset.metadata = metadata;

    // Calculate memory size
    asset.memorySize = calculateShaderMemorySize(asset);

    // Store in temporary key until prepareAsset can store with the right filename
    std::string tempKey = "temp_shader_" + std::to_string(handle.id);
    m_shaderAssets[tempKey] = std::move(asset);

    SPDLOG_INFO("Shader created successfully (handle: {})", handle.id);
    return handle;
}

void ShaderManager::destroyShader(ShaderHandle handle) {
    // Find the shader asset
    ShaderAsset* assetPtr = nullptr;
    std::string assetFilename;

    for (auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            assetPtr = &asset;
            assetFilename = filename;
            break;
        }
    }

    if (assetPtr) {
        // First, destroy the pipeline layout
        if (assetPtr->resources.pipelineLayout) {
            m_pipelineLayoutManager.destroy(assetPtr->resources.pipelineLayout);
        }

        // Destroy descriptor layouts (but skip built-in ones)
        for (const auto& [set, layoutHandle] : assetPtr->resources.descriptorLayouts) {
            if (layoutHandle) {
                // Don't destroy built-in layouts
                if (set != ShaderLib::GLOBAL_DESCRIPTOR_SET &&
                    set != ShaderLib::OBJECT_DESCRIPTOR_SET) {
                    m_descriptorLayoutManager.destroy(layoutHandle);
                }
            }
        }

        // Destroy all shader modules
        for (const auto& [stage, moduleHandle] : assetPtr->combinedShader.stages) {
            m_shaderModuleManager.destroyModule(moduleHandle);
        }
    }
}

std::unordered_map<uint32_t, DescriptorLayoutHandle> ShaderManager::createDescriptorLayouts(
    const ShaderLib::ShaderMetadata& metadata
) {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> result;

    // Group descriptors by set
    std::unordered_map<uint32_t, std::vector<ShaderLib::DescriptorBinding>> descriptorsBySet;

    for (const auto& descriptor : metadata.descriptors) {
        descriptorsBySet[descriptor.set].push_back(descriptor);
    }

    // Create descriptor layouts for each set
    for (const auto& [set, descriptors] : descriptorsBySet) {
        DescriptorLayoutHandle layoutHandle;

        // Use built-in layouts for Global and Object descriptor sets
        if (set == ShaderLib::GLOBAL_DESCRIPTOR_SET && metadata.usesGlobalUBO) {
            layoutHandle = m_descriptorLayoutManager.getBuiltInLayout(
                DescriptorLayoutManager::BuiltInLayout::Global
            );
            SPDLOG_DEBUG("Using built-in Global descriptor layout for set {}", set);
        }
        else if (set == ShaderLib::OBJECT_DESCRIPTOR_SET && metadata.usesObjectUBO) {
            layoutHandle = m_descriptorLayoutManager.getBuiltInLayout(
                DescriptorLayoutManager::BuiltInLayout::Object
            );
            SPDLOG_DEBUG("Using built-in Object descriptor layout for set {}", set);
        }
        else {
            // Create custom descriptor layout for other sets
            DescriptorLayoutBuilder builder;
            VkShaderStageFlags combinedStageFlags = 0;

            // Add bindings to builder
            for (const auto& descriptor : descriptors) {
                VkDescriptorType vulkanDescriptorType =
                    static_cast<VkDescriptorType>(ShaderLib::GetVulkanDescriptorType(descriptor.descriptorType));

                // Convert the stage flags for this descriptor
                VkShaderStageFlags stageFlags = ShaderLib::GetVulkanShaderStageFlags(descriptor.stages);
                combinedStageFlags |= stageFlags;

                builder.addBinding(descriptor.binding, vulkanDescriptorType);
            }

            // Create descriptor layout
            layoutHandle = m_descriptorLayoutManager.create(
                builder,
                combinedStageFlags
            );
            SPDLOG_DEBUG("Created custom descriptor layout for set {}", set);
        }

        result[set] = layoutHandle;
    }

    return result;
}

PipelineLayoutHandle ShaderManager::createPipelineLayout(
    const ShaderLib::ShaderMetadata& metadata,
    const std::unordered_map<uint32_t, DescriptorLayoutHandle>& descriptorLayouts
) {
    // Create pipeline layout configuration
    PipelineLayoutConfig layoutConfig;

    // Convert push constants
    layoutConfig.pushConstantRanges.reserve(metadata.pushConstants.size());
    for (const auto& pushConstant : metadata.pushConstants) {
        VkPushConstantRange range;
        range.stageFlags = ShaderLib::GetVulkanShaderStageFlags(pushConstant.stages);
        range.offset = pushConstant.offset;
        range.size = pushConstant.size;
        layoutConfig.pushConstantRanges.push_back(range);
    }

    // Collect descriptor layouts in order
    std::vector<VkDescriptorSetLayout> vkLayouts;

    // Find the highest set number to properly size our collection
    uint32_t maxSetNumber = 0;
    for (const auto& [set, _] : descriptorLayouts) {
        maxSetNumber = std::max(maxSetNumber, set);
    }

    // Reserve space for all sets up to maxSetNumber
    vkLayouts.resize(maxSetNumber + 1, VK_NULL_HANDLE);

    // Fill in the layouts we have
    for (const auto& [set, layoutHandle] : descriptorLayouts) {
        if (set <= maxSetNumber) {
            vkLayouts[set] = m_descriptorLayoutManager.get(layoutHandle);
        }
    }

    // Remove any nullptr layouts at the end
    while (!vkLayouts.empty() && vkLayouts.back() == VK_NULL_HANDLE) {
        vkLayouts.pop_back();
    }

    layoutConfig.descriptorSetLayouts = std::move(vkLayouts);

    return m_pipelineLayoutManager.createLayout(layoutConfig);
}

uint64_t ShaderManager::calculateShaderMemorySize(const ShaderAsset& asset) const {
    uint64_t size = 0;

    // Sum up the actual size of SPIR-V code for all modules in this shader
    for (const auto& [stage, moduleHandle] : asset.combinedShader.stages) {
        // Approximate size for shader module
        size += 4096; // Base overhead
    }

    // Add descriptor layouts size
    size += asset.resources.descriptorLayouts.size() * 256;

    // Add pipeline layout size
    size += 512;

    // Add fixed overhead for shader asset
    size += 1024;

    return size;
}

std::string ShaderManager::getShaderCacheKey(const std::string& filename) const {
    return filename;
}