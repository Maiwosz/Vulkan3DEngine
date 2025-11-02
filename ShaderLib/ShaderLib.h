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

namespace ShaderLib {
    constexpr uint32_t GLOBAL_DESCRIPTOR_SET = 0;
    constexpr uint32_t OBJECT_DESCRIPTOR_SET = 1;
    constexpr uint32_t CUSTOM_DESCRIPTOR_SET = 2;

    constexpr uint32_t GLOBAL_UBO_BINDING = 0;
    constexpr uint32_t OBJECT_UBO_BINDING = 0;

    constexpr uint32_t INPUT_DATA_BINDING = 0;
    constexpr uint32_t OUTPUT_DATA_BINDING = 1;
    constexpr uint32_t INPUT_OUTPUT_DATA_BINDING = 2;
    constexpr uint32_t SAMPLERS_START_BINDING = 3;

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
    // BUFFER ACCESS MODE
    // ============================================================================

    enum class BufferAccessMode : uint8_t {
        ReadOnly,   // uniform buffers, readonly storage buffers
        WriteOnly,  // writeonly storage buffers
        ReadWrite   // read/write storage buffers
    };

    // ============================================================================
    // BUFFER VARIABLE DEFINITION
    // ============================================================================

    struct BufferVariable {
        std::string name;
        BaseType baseType;
        std::shared_ptr<const CompositeTypeDefinition> composite;
        uint32_t size;
        uint32_t offset;
        BufferAccessMode accessMode;

        // Default constructor for deserialization
        BufferVariable()
            : name(""), baseType(BaseType::Unknown), composite(nullptr),
            size(0), offset(0), accessMode(BufferAccessMode::ReadOnly) {
        }

        // Constructor for base types
        BufferVariable(const std::string& n, BaseType t, uint32_t sz, uint32_t off,
            BufferAccessMode mode = BufferAccessMode::ReadWrite)
            : name(n), baseType(t), composite(nullptr),
            size(sz), offset(off), accessMode(mode) {
        }

        // Constructor for composite types
        BufferVariable(const std::string& n, std::shared_ptr<const CompositeTypeDefinition> comp,
            uint32_t off, BufferAccessMode mode = BufferAccessMode::ReadWrite)
            : name(n),
            baseType(comp->IsStruct() ? BaseType::Struct : BaseType::Array),
            composite(comp),
            size(comp->GetSize()),
            offset(off),
            accessMode(mode) {
        }

        // Inline helper methods
        bool IsBase() const {
            return !IsComposite() && baseType != BaseType::Unknown;
        }

        bool IsComposite() const {
            return composite != nullptr;
        }

        bool IsReadOnly() const {
            return accessMode == BufferAccessMode::ReadOnly;
        }

        bool IsWriteOnly() const {
            return accessMode == BufferAccessMode::WriteOnly;
        }

        bool IsReadWrite() const {
            return accessMode == BufferAccessMode::ReadWrite;
        }

        // Complex helpers
        std::string GetTypeName() const;
        ShaderTypeCategory GetCategory() const;
        std::string GenerateGLSLDeclaration() const;
    };

    // ============================================================================
    // BUFFER TYPE
    // ============================================================================

    enum class BufferType {
        Uniform,  // UBO - always ReadOnly
        Storage   // SSBO - can be ReadOnly, WriteOnly, or ReadWrite
    };

    // ============================================================================
    // BUFFER OBJECTS (UBO/SSBO)
    // ============================================================================

    struct BufferObject {
        std::string name;
        uint32_t size;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        BufferAccessMode accessMode;
        std::vector<BufferVariable> variables;
        bool useInstanceName = true;

        // Inline helper methods
        bool IsUniformBuffer() const {
            return bufferType == BufferType::Uniform;
        }

        bool IsStorageBuffer() const {
            return bufferType == BufferType::Storage;
        }

        bool IsReadOnly() const {
            return accessMode == BufferAccessMode::ReadOnly;
        }

        bool IsWriteOnly() const {
            return accessMode == BufferAccessMode::WriteOnly;
        }

        bool IsReadWrite() const {
            return accessMode == BufferAccessMode::ReadWrite;
        }

        // Complex helper - moved to .cpp
        std::string GetAccessQualifier() const;
    };

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
        std::unordered_map<std::string, BufferObject> buffers;

        // Search/lookup methods - moved to .cpp
        const DescriptorSlot* FindSlot(uint32_t binding) const;
        DescriptorSlot* FindSlot(uint32_t binding);
        const DescriptorSlot* FindSlot(const std::string& name) const;

        const BufferObject* GetBuffer(const std::string& name) const;
        BufferObject* GetBuffer(const std::string& name);
        const BufferObject* GetBufferByBinding(uint32_t binding) const;
        BufferObject* GetBufferByBinding(uint32_t binding);

        // Validation methods - moved to .cpp
        bool HasBindingConflict() const;
        bool ValidateBuffers() const;

        // Collection methods - moved to .cpp
        std::vector<const BufferObject*> GetAllBuffers() const;
        std::vector<const DescriptorSlot*> GetAllSamplers() const;
        std::vector<const DescriptorSlot*> GetSlotsByType(DescriptorType type) const;

        // Generate complete GLSL for entire descriptor set
        std::string GenerateGLSL() const;

        // Helper function for buffer generation (could be private static method)
        std::string GenerateBufferGLSL(
            const BufferObject& buffer,
            uint32_t set,
            uint32_t binding
        ) const;

        // Helper function for sampler/image generation
        std::string GenerateSamplerGLSL(
            const DescriptorSlot& slot,
            uint32_t set
        ) const;
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
        std::vector<BufferObject> customBuffers;

        BufferObject globalUBO;
        BufferObject objectUBO;

        std::optional<ComputeShaderInfo> computeInfo;

        // Search/lookup methods - moved to .cpp
        const DescriptorSet* GetSet(uint32_t setNumber) const;
        DescriptorSet* GetSet(uint32_t setNumber);
        const DescriptorSlot* FindDescriptor(const std::string& name) const;
        const BufferObject* FindBuffer(const std::string& name) const;
        BufferObject* FindBuffer(const std::string& name);

        // Inline convenience methods
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

        // Collection and validation methods - moved to .cpp
        std::vector<const BufferObject*> GetAllBuffers() const;
        std::vector<const DescriptorSlot*> GetAllSamplers() const;
        bool ValidateDescriptorSets() const;
        std::vector<BufferObject> GetCustomUniformBuffers() const;
        std::vector<BufferObject> GetCustomStorageBuffers() const;
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