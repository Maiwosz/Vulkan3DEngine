#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <array>
#include <glm/glm.hpp>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <json.hpp>

namespace ShaderLib {
    constexpr uint32_t GLOBAL_DESCRIPTOR_SET = 0;
    constexpr uint32_t OBJECT_DESCRIPTOR_SET = 1;
    constexpr uint32_t CUSTOM_DESCRIPTOR_SET = 2;

    enum class Stage : uint32_t {
        Vertex = 1 << 0,
        Fragment = 1 << 1,
        Compute = 1 << 2,
        Geometry = 1 << 3,
        TessellationControl = 1 << 4,
        TessellationEvaluation = 1 << 5
    };
    using StageFlags = uint32_t;

    enum class DescriptorType {
        UniformBuffer,
        StorageBuffer,
        CombinedImageSampler,
        SeparateImage,
        SeparateSampler
    };

    // Nowy enum dla typu bufora
    enum class BufferType {
        Uniform,  // UBO - read-only, std140
        Storage   // SSBO - read-write, std430
    };

    // Nowy enum dla standardu layoutu
    enum class LayoutStandard {
        Std140,  // For uniform buffers
        Std430   // For storage buffers (more compact)
    };

    enum class UniformType {
        Bool,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4,
        Int,
        IVec2,
        IVec3,
        IVec4,
        UInt,
        UVec2,
        UVec3,
        UVec4,
        Double,
        DVec2,
        DVec3,
        DVec4,
        Struct,
        Array,
        Unknown
    };

    // Size and alignment information for standard types
    struct TypeInfo {
        uint32_t size;
        uint32_t alignment;
    };

    struct UniformVariable {
        std::string name;
        UniformType type;
        uint32_t size;
        uint32_t offset;
        uint32_t arraySize; // 0 for non-array types
        std::string typeName; // For struct types
    };

    // Structure to define Light types referenced in GlobalUBO
    struct LightTypeInfo {
        std::string name;
        std::vector<UniformVariable> members;
        uint32_t size;
        uint32_t alignment;
    };

    struct PushConstantRange {
        StageFlags stages;
        uint32_t offset;
        uint32_t size;
    };

    struct DescriptorBinding {
        uint32_t set;
        uint32_t binding;
        DescriptorType type;
        StageFlags stages;
        std::string name;
    };

    // Renamed from UniformBufferObject to BufferObject for universality
    struct BufferObject {
        std::string name;
        uint32_t set;
        uint32_t binding;
        uint32_t size;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        std::vector<UniformVariable> variables;

        // Helper methods
        bool IsUniformBuffer() const { return bufferType == BufferType::Uniform; }
        bool IsStorageBuffer() const { return bufferType == BufferType::Storage; }
    };

    // Alias dla kompatybilności wstecznej
    using UniformBufferObject = BufferObject;

    struct ShaderMetadata {
        StageFlags availableStages;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;
        std::vector<PushConstantRange> pushConstants;
        std::vector<DescriptorBinding> descriptors;
        std::vector<BufferObject> customUBOs;
        std::vector<BufferObject> customSSBOs;

        // Defined structures
        BufferObject globalUBO;
        BufferObject objectUBO;
    };

    struct CompiledStage {
        std::vector<uint32_t> spirv;
        Stage stage;
    };

    struct ShaderData {
        nlohmann::json metadata;
        std::vector<CompiledStage> stages;
    };
}