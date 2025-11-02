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

    // Clean up empty layout cache
    for (const auto& [_, handle] : m_emptyLayoutCache) {
        m_descriptorLayoutManager.destroy(handle);
    }
    m_emptyLayoutCache.clear();
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

ShaderAsset* ShaderManager::getResource(ShaderHandle handle) const {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            return const_cast<ShaderAsset*>(&asset);
        }
    }
    return nullptr;
}

bool ShaderManager::isAssetReady(ShaderHandle handle) const {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == handle) {
            return true;
        }
    }
    return false;
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

ShaderModuleHandle ShaderManager::getModuleHandleForStage(ShaderHandle shader, ShaderLib::Stage stage) {
    for (const auto& [filename, asset] : m_shaderAssets) {
        if (asset.handle == shader) {
            auto it = asset.combinedShader.stages.find(stage);
            if (it != asset.combinedShader.stages.end()) {
                return it->second;
            }
            return ShaderModuleHandle();
        }
    }
    return ShaderModuleHandle();
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
    // Log shader metadata
    SPDLOG_DEBUG("Shader metadata:");
    SPDLOG_DEBUG("  - Uses GlobalUBO: {}", metadata.usesGlobalUBO ? "yes" : "no");
    SPDLOG_DEBUG("  - Uses ObjectUBO: {}", metadata.usesObjectUBO ? "yes" : "no");
    SPDLOG_DEBUG("  - Descriptor sets: {}", metadata.descriptorSets.size());
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

        // Destroy descriptor layouts (but skip built-in ones and empty ones)
        for (const auto& [set, layoutHandle] : assetPtr->resources.descriptorLayouts) {
            if (layoutHandle) {
                // Don't destroy built-in layouts
                if (set != ShaderLib::GLOBAL_DESCRIPTOR_SET &&
                    set != ShaderLib::OBJECT_DESCRIPTOR_SET) {

                    // Check if it's an empty layout from cache
                    bool isEmptyLayout = false;
                    for (const auto& [_, emptyHandle] : m_emptyLayoutCache) {
                        if (layoutHandle == emptyHandle) {
                            isEmptyLayout = true;
                            break;
                        }
                    }

                    // Only destroy if it's not an empty layout
                    if (!isEmptyLayout) {
                        m_descriptorLayoutManager.destroy(layoutHandle);
                    }
                }
            }
        }

        // Destroy all shader modules
        for (const auto& [stage, moduleHandle] : assetPtr->combinedShader.stages) {
            m_shaderModuleManager.destroyModule(moduleHandle);
        }
    }
}

DescriptorLayoutHandle ShaderManager::createEmptyDescriptorLayout() {
    // Check if we already have an empty layout cached
    if (!m_emptyLayoutCache.empty()) {
        return m_emptyLayoutCache.begin()->second;
    }

    // Create empty descriptor layout (no bindings)
    DescriptorLayoutBuilder builder;
    // Don't add any bindings - this creates an empty layout

    DescriptorLayoutHandle handle = m_descriptorLayoutManager.create(
        builder,
        VK_SHADER_STAGE_ALL
    );

    // Cache it for reuse
    m_emptyLayoutCache[0] = handle;

    SPDLOG_DEBUG("Created empty descriptor layout (handle: {})", handle.id);
    return handle;
}

std::unordered_map<uint32_t, DescriptorLayoutHandle> ShaderManager::createDescriptorLayouts(
    const ShaderLib::ShaderMetadata& metadata
) {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> result;

    // Find the maximum set number used
    uint32_t maxSetNumber = 0;
    for (const auto& descriptorSet : metadata.descriptorSets) {
        maxSetNumber = std::max(maxSetNumber, descriptorSet.setNumber);
    }

    SPDLOG_DEBUG("Creating descriptor layouts for sets 0 to {}", maxSetNumber);

    // Create layouts for all sets from 0 to maxSetNumber (filling gaps with empty layouts)
    for (uint32_t setNum = 0; setNum <= maxSetNumber; ++setNum) {
        DescriptorLayoutHandle layoutHandle;
        bool foundSet = false;

        // Check if this set is defined in metadata
        for (const auto& descriptorSet : metadata.descriptorSets) {
            if (descriptorSet.setNumber == setNum) {
                foundSet = true;

                // Use built-in layouts for Global and Object descriptor sets
                if (setNum == ShaderLib::GLOBAL_DESCRIPTOR_SET && metadata.usesGlobalUBO) {
                    layoutHandle = m_descriptorLayoutManager.getBuiltInLayout(
                        DescriptorLayoutManager::BuiltInLayout::Global
                    );
                    SPDLOG_DEBUG("Using built-in Global descriptor layout for set {}", setNum);
                }
                else if (setNum == ShaderLib::OBJECT_DESCRIPTOR_SET && metadata.usesObjectUBO) {
                    layoutHandle = m_descriptorLayoutManager.getBuiltInLayout(
                        DescriptorLayoutManager::BuiltInLayout::Object
                    );
                    SPDLOG_DEBUG("Using built-in Object descriptor layout for set {}", setNum);
                }
                else {
                    // Create custom descriptor layout for other sets
                    DescriptorLayoutBuilder builder;
                    VkShaderStageFlags combinedStageFlags = 0;

                    // Add bindings from slots
                    for (const auto& slot : descriptorSet.slots) {
                        VkDescriptorType vulkanDescriptorType =
                            static_cast<VkDescriptorType>(ShaderLib::GetVulkanDescriptorType(slot.type));

                        // Convert the stage flags for this slot
                        VkShaderStageFlags stageFlags = ShaderLib::GetVulkanShaderStageFlags(slot.stages);
                        combinedStageFlags |= stageFlags;

                        builder.addBinding(slot.binding, vulkanDescriptorType);
                    }

                    // Create descriptor layout
                    layoutHandle = m_descriptorLayoutManager.create(
                        builder,
                        combinedStageFlags
                    );
                    SPDLOG_DEBUG("Created custom descriptor layout for set {} with {} bindings",
                        setNum, descriptorSet.slots.size());
                }
                break;
            }
        }

        // If set not found, create empty layout to fill the gap
        if (!foundSet) {
            layoutHandle = createEmptyDescriptorLayout();
            SPDLOG_DEBUG("Using empty descriptor layout for unused set {}", setNum);
        }

        result[setNum] = layoutHandle;
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

    if (!descriptorLayouts.empty()) {
        // Find the maximum set number
        uint32_t maxSetNumber = 0;
        for (const auto& [set, _] : descriptorLayouts) {
            maxSetNumber = std::max(maxSetNumber, set);
        }

        SPDLOG_DEBUG("Creating pipeline layout with descriptor sets 0 to {}", maxSetNumber);

        // Build contiguous array from 0 to maxSetNumber
        // At this point, createDescriptorLayouts should have filled all gaps
        vkLayouts.resize(maxSetNumber + 1);

        for (uint32_t set = 0; set <= maxSetNumber; ++set) {
            auto it = descriptorLayouts.find(set);
            if (it == descriptorLayouts.end()) {
                SPDLOG_ERROR("Missing descriptor layout for set {} - this should not happen!", set);
                throw std::runtime_error("Missing descriptor layout for set " + std::to_string(set));
            }

            // Verify the handle is valid
            if (!it->second) {
                SPDLOG_ERROR("Descriptor set {} has invalid handle (id={})", set, it->second.id);
                throw std::runtime_error("Invalid descriptor layout handle for set " + std::to_string(set));
            }

            VkDescriptorSetLayout layout = m_descriptorLayoutManager.get(it->second);
            if (layout == VK_NULL_HANDLE) {
                SPDLOG_ERROR("Descriptor set {} resolved to VK_NULL_HANDLE (handle id={})",
                    set, it->second.id);
                throw std::runtime_error("Invalid descriptor layout for set " + std::to_string(set));
            }

            vkLayouts[set] = layout;
            SPDLOG_DEBUG("Added descriptor set {} to pipeline layout", set);
        }
    }

    SPDLOG_INFO("Pipeline layout created with {} descriptor set layouts", vkLayouts.size());
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