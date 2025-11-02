#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <memory>
#include <variant>
#include "ShaderTypes.h"
#include "ShaderDescriptors.h"

namespace AssetLib {

    // ============================================================================
    // COLOR SPACE & SAMPLER DESCRIPTION
    // ============================================================================

    enum class ColorSpace : uint8_t {
        Linear = 0,
        SRGB = 1,
        HDR = 2
    };

    struct SamplerDescription {
        enum class Filter : uint8_t {
            Nearest = 0,
            Linear = 1
        };

        enum class AddressMode : uint8_t {
            Repeat = 0,
            MirroredRepeat = 1,
            ClampToEdge = 2,
            ClampToBorder = 3
        };

        Filter magFilter = Filter::Linear;
        Filter minFilter = Filter::Linear;
        AddressMode addressModeU = AddressMode::Repeat;
        AddressMode addressModeV = AddressMode::Repeat;
        AddressMode addressModeW = AddressMode::Repeat;
        float anisotropy = 1.0f;
        float minLod = 0.0f;
        float maxLod = 1000.0f;
        ColorSpace colorSpace = ColorSpace::Linear;

        bool operator==(const SamplerDescription& other) const = default;
    };

    // ============================================================================
    // PARAMETER VALUE TYPES
    // ============================================================================

    enum class ParameterValueType : uint8_t {
        BaseType = 0,      // Simple types (float, vec3, etc.)
        Struct = 1,        // ShaderStruct
        Array = 2,         // ShaderArray
        TexturePath = 3    // String path to texture
    };

    // ============================================================================
    // MATERIAL DEPENDENCIES (stored in metadata)
    // ============================================================================

    struct MaterialDependencies {
        std::string shaderName;                          // Shader this material uses
        std::vector<std::string> textureNames;           // Textures this material uses
        std::vector<ColorSpace> textureColorSpaces;      // Color space for each texture
    };

    // ============================================================================
    // MATERIAL PARAMETER VALUE (high-level representation)
    // ============================================================================

    struct ParameterValue {
        using ValueVariant = std::variant<
            ShaderLib::BufferValue,    // For BaseType, Struct, Array (BufferValue already contains shared_ptr for composites)
            std::string                // For TexturePath
        >;

        std::string name;
        ShaderLib::DescriptorType descriptorType;
        ParameterValueType valueType;
        ShaderLib::BaseType baseType = ShaderLib::BaseType::Unknown;  // Only for BaseType
        uint32_t arraySize = 1;                                        // For descriptor arrays
        SamplerDescription samplerDesc;                                // For textures/samplers
        ValueVariant value;

        // Helper methods
        bool IsTexture() const;
        bool IsBuffer() const;
        bool IsSampler() const;
        bool IsBaseType() const { return valueType == ParameterValueType::BaseType; }
        bool IsComposite() const { return valueType == ParameterValueType::Struct || valueType == ParameterValueType::Array; }
        bool IsTexturePath() const { return valueType == ParameterValueType::TexturePath; }

        // Get typed values
        const ShaderLib::BufferValue& GetBaseValue() const;
        std::shared_ptr<ShaderLib::CompositeTypeInstance> GetComposite() const;
        const std::string& GetTexturePath() const;

        // Validation
        bool Validate() const;
    };

    // ============================================================================
    // MATERIAL DEFINITION (high-level representation for serialization)
    // ============================================================================

    struct MaterialDefinition {
        std::string shaderName;
        std::vector<ParameterValue> parameters;

        // Helper methods
        const ParameterValue* FindParameter(const std::string& name) const;
        ParameterValue* FindParameter(const std::string& name);
        bool HasParameter(const std::string& name) const;

        // Validation
        bool Validate() const;

        // Get parameters by type
        std::vector<const ParameterValue*> GetTextureParameters() const;
        std::vector<const ParameterValue*> GetBufferParameters() const;
        std::vector<const ParameterValue*> GetSamplerParameters() const;

        // Extract dependencies for metadata
        MaterialDependencies ExtractDependencies() const;
    };

    // ============================================================================
    // BINARY MATERIAL FORMAT (low-level for file storage)
    // ============================================================================

#pragma pack(push, 1)

    struct MaterialParameter {
        std::array<char, 32> name;
        ShaderLib::DescriptorType descriptorType;
        ParameterValueType valueType;
        ShaderLib::BaseType baseType;
        uint32_t arraySize;
        SamplerDescription samplerDesc;
        uint32_t dataOffset;        // Offset into binary data blob
        uint32_t dataSize;          // Size in binary data blob
        uint32_t compositeDefOffset; // Offset in composite definitions string
        uint32_t compositeDefSize;   // Size in composite definitions string
    };

    struct MaterialInfo {
        std::array<char, 32> shaderName;
        uint32_t parameterCount;
        uint32_t dataSize;           // Total binary data size
        uint32_t compositeDefsSize;  // Total composite definitions size
    };

#pragma pack(pop)

    // ============================================================================
    // MATERIAL DATA (intermediate format after reading from file)
    // ============================================================================

    struct MaterialData {
        MaterialInfo info;
        std::vector<MaterialParameter> parameters;
        std::vector<uint8_t> parameterData;
        std::string compositeDefinitions;  // JSON string with all composite type definitions
    };

    // ============================================================================
    // CONVERSION HELPER FUNCTIONS
    // ============================================================================

    // String conversions
    std::string SamplerFilterToString(SamplerDescription::Filter filter);
    std::string AddressModeToString(SamplerDescription::AddressMode mode);
    std::string ColorSpaceToString(ColorSpace colorSpace);
    std::string ParameterValueTypeToString(ParameterValueType type);

    SamplerDescription::Filter StringToSamplerFilter(const std::string& filter);
    SamplerDescription::AddressMode StringToAddressMode(const std::string& mode);
    ColorSpace StringToColorSpace(const std::string& colorSpace);
    ParameterValueType StringToParameterValueType(const std::string& str);

    // Default sampler
    SamplerDescription GetDefaultSampler();

} // namespace AssetLib