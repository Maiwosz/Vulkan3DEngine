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

namespace ShaderLib {
    constexpr uint32_t GLOBAL_DESCRIPTOR_SET = 0;
    constexpr uint32_t OBJECT_DESCRIPTOR_SET = 1;
    constexpr uint32_t CUSTOM_DESCRIPTOR_SET = 2;

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
    // BUFFER VARIABLE DEFINITION
    // ============================================================================

    // ============================================================================
// BUFFER VARIABLE DEFINITION
// ============================================================================

    struct BufferVariable {
        std::string name;
        BaseType baseType;  // Może być Struct/Array
        std::shared_ptr<CompositeType> composite;  // Wypełnione tylko gdy IsComposite()
        uint32_t size;    // Runtime dla composite
        uint32_t offset;

        // Default constructor for deserialization
        BufferVariable()
            : name(""), baseType(BaseType::Unknown), composite(nullptr), size(0), offset(0) {
        }

        BufferVariable(const std::string& n, BaseType t, uint32_t sz, uint32_t off)
            : name(n), baseType(t), composite(nullptr), size(sz), offset(off) {
        }

        BufferVariable(const std::string& n, std::shared_ptr<CompositeType> comp, uint32_t off)
            : name(n),
            baseType(comp->IsStruct() ? BaseType::Struct : BaseType::Array),
            composite(comp),
            size(comp->GetSize()),
            offset(off) {
        }

        // Helper methods
        bool IsBase() const {
            return !IsComposite() && baseType != BaseType::Unknown;
        }

        bool IsComposite() const {
            return composite != nullptr;
        }

        std::string GetTypeName() const {
            if (IsComposite()) {
                return composite->GetTypeName();
            }
            return BaseTypeToString(baseType);
        }

        ShaderTypeCategory GetCategory() const {
            if (IsComposite()) {
                return ShaderTypeCategory::Composite;
            }
            return (baseType != BaseType::Unknown)
                ? ShaderTypeCategory::Base
                : ShaderTypeCategory::Unknown;
        }
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
    // DESCRIPTOR BINDINGS
    // ============================================================================

    struct DescriptorBinding {
        uint32_t set;
        uint32_t binding;
        DescriptorType descriptorType;
        StageFlags stages;
        std::string name;
    };

    // ============================================================================
    // BUFFER TYPE
    // ============================================================================

    enum class BufferType {
        Uniform,  // UBO
        Storage   // SSBO
    };

    // ============================================================================
    // BUFFER OBJECTS (UBO/SSBO)
    // ============================================================================

    struct BufferObject {
        std::string name;
        uint32_t set;
        uint32_t binding;
        uint32_t size;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        std::vector<BufferVariable> variables;

        bool IsUniformBuffer() const {
            return bufferType == BufferType::Uniform;
        }

        bool IsStorageBuffer() const {
            return bufferType == BufferType::Storage;
        }
    };

    // ============================================================================
    // SHADER METADATA
    // ============================================================================

    struct ShaderMetadata {
        StageFlags availableStages;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;
        std::vector<PushConstantRange> pushConstants;
        std::vector<DescriptorBinding> descriptors;
        std::vector<BufferObject> customBuffers;

        // Predefined standard buffers
        BufferObject globalUBO;
        BufferObject objectUBO;

        std::vector<BufferObject> GetCustomUniformBuffers() const {
            std::vector<BufferObject> result;
            for (const auto& buf : customBuffers) {
                if (buf.IsUniformBuffer()) {
                    result.push_back(buf);
                }
            }
            return result;
        }

        std::vector<BufferObject> GetCustomStorageBuffers() const {
            std::vector<BufferObject> result;
            for (const auto& buf : customBuffers) {
                if (buf.IsStorageBuffer()) {
                    result.push_back(buf);
                }
            }
            return result;
        }
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