#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <array>
#include <memory>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <json.hpp>
#include "ShaderDescriptors.h"
#include <set>
#include "BufferObjectDefinition.h"

namespace ShaderLib {
    // Tylko standardowe sety - bez predefiniowanych bindingów
    constexpr uint32_t GLOBAL_DESCRIPTOR_SET = 0;
    constexpr uint32_t OBJECT_DESCRIPTOR_SET = 1;
    constexpr uint32_t CUSTOM_DESCRIPTOR_SET = 2;

    // Tylko standardowe bindingi dla global/object
    constexpr uint32_t GLOBAL_UBO_BINDING = 0;
    constexpr uint32_t OBJECT_UBO_BINDING = 0;

    // ============================================================================
    // SHADER STAGES
    // ============================================================================

    enum class Stage : uint32_t {
        Vertex = 1 << 0,
        Fragment = 1 << 1,
        Compute = 1 << 2,
        Geometry = 1 << 3,
        TessellationControl = 1 << 4,
        TessellationEvaluation = 1 << 5
    };
    using StageFlags = uint32_t;

    // ============================================================================
    // PUSH CONSTANTS
    // ============================================================================

    struct PushConstantRange {
        StageFlags stages;
        uint32_t offset;
        uint32_t size;
    };

    // ============================================================================
    // DESCRIPTOR SLOT - Single binding within a set
    // ============================================================================

    struct DescriptorSlot {
        uint32_t binding;
        DescriptorType type;
        StageFlags stages;
        std::string name;

        // Inline helper methods
        bool IsBuffer() const {
            return type == DescriptorType::UniformBuffer ||
                type == DescriptorType::StorageBuffer;
        }

        bool IsSampler() const {
            return !IsBuffer();
        }

        bool IsUniformBuffer() const {
            return type == DescriptorType::UniformBuffer;
        }

        bool IsStorageBuffer() const {
            return type == DescriptorType::StorageBuffer;
        }
    };

    // ============================================================================
    // DESCRIPTOR SET - Collection of bindings
    // ============================================================================

    struct DescriptorSet {
        uint32_t setNumber;
        std::vector<DescriptorSlot> slots;
        std::unordered_map<std::string, std::shared_ptr<BufferObjectDefinition>> buffers;

        // Search/lookup methods - tylko podstawowe wyszukiwanie
        const DescriptorSlot* FindSlot(uint32_t binding) const;
        DescriptorSlot* FindSlot(uint32_t binding);
        const DescriptorSlot* FindSlot(const std::string& name) const;

        std::shared_ptr<const BufferObjectDefinition> GetBuffer(const std::string& name) const;
        std::shared_ptr<BufferObjectDefinition> GetBuffer(const std::string& name);
        std::shared_ptr<const BufferObjectDefinition> GetBufferByBinding(uint32_t binding) const;
        std::shared_ptr<BufferObjectDefinition> GetBufferByBinding(uint32_t binding);

        // Validation methods
        bool HasBindingConflict() const;
        bool ValidateBuffers() const;

        // Collection methods
        std::vector<std::shared_ptr<const BufferObjectDefinition>> GetAllBuffers() const;
        std::vector<const DescriptorSlot*> GetAllSamplers() const;
        std::vector<const DescriptorSlot*> GetSlotsByType(DescriptorType type) const;

        // Generate complete GLSL for entire descriptor set
        std::string GenerateGLSL() const;

    private:
        // Helper function for sampler/image generation
        std::string GenerateSamplerGLSL(const DescriptorSlot& slot, uint32_t set) const;
    };

    // ============================================================================
    // SHADER METADATA
    // ============================================================================

    struct ComputeShaderInfo {
        uint32_t localSizeX = 1;
        uint32_t localSizeY = 1;
        uint32_t localSizeZ = 1;

        bool usesSharedMemory = false;
        bool usesAtomics = false;
        bool usesBarriers = false;

        uint32_t sharedMemorySize = 0;

        // Inline helper
        uint32_t GetTotalWorkgroupSize() const {
            return localSizeX * localSizeY * localSizeZ;
        }
    };

    struct ShaderMetadata {
        StageFlags availableStages;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;
        std::vector<PushConstantRange> pushConstants;
        std::vector<DescriptorSet> descriptorSets;
        std::optional<ComputeShaderInfo> computeInfo;

        // Search/lookup methods - tylko podstawowe
        const DescriptorSet* GetSet(uint32_t setNumber) const;
        DescriptorSet* GetSet(uint32_t setNumber);
        const DescriptorSlot* FindDescriptor(const std::string& name) const;
        std::shared_ptr<const BufferObjectDefinition> FindBuffer(const std::string& name) const;
        std::shared_ptr<BufferObjectDefinition> FindBuffer(const std::string& name);

        // Convenience methods dla standardowych setów
        const DescriptorSet* GetGlobalSet() const {
            return GetSet(GLOBAL_DESCRIPTOR_SET);
        }

        const DescriptorSet* GetObjectSet() const {
            return GetSet(OBJECT_DESCRIPTOR_SET);
        }

        const DescriptorSet* GetCustomSet() const {
            return GetSet(CUSTOM_DESCRIPTOR_SET);
        }

        bool IsComputeShader() const {
            return availableStages & static_cast<uint32_t>(Stage::Compute);
        }

        // Collection and validation methods
        std::vector<std::shared_ptr<const BufferObjectDefinition>> GetAllBuffers() const;
        std::vector<const DescriptorSlot*> GetAllSamplers() const;
        bool ValidateDescriptorSets() const;

        // Tylko dla Global i Object UBO - te są specjalne
        std::shared_ptr<const BufferObjectDefinition> GetGlobalUBO() const;
        std::shared_ptr<BufferObjectDefinition> GetGlobalUBO();
        std::shared_ptr<const BufferObjectDefinition> GetObjectUBO() const;
        std::shared_ptr<BufferObjectDefinition> GetObjectUBO();
    };

    // ============================================================================
    // COMPILED SHADER DATA
    // ============================================================================

    struct CompiledStage {
        std::vector<uint32_t> spirv;
        Stage stage;
    };

    struct ShaderData {
        nlohmann::json metadata;
        std::vector<CompiledStage> stages;
    };

} // namespace ShaderLib
