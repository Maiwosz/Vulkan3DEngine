//#pragma once
//#include "AssetLib.h"
//#include "Material.h"
//#include "ShaderLib.h"
//#include "MaterialTypes.h"
//#include <string>
//#include <vector>
//#include <memory>
//#include <optional>
//
///**
// * MaterialCreator - High-level API for creating and serializing materials
// *
// * Key responsibilities:
// * - Convert from high-level Material::ParamValue to AssetLib serialization format
// * - Validate material definitions against shader metadata
// * - Generate default parameter values from shader metadata
// * - Handle file I/O and compression
// */
//class MaterialCreator {
//public:
//    // ============================================================================
//    // MATERIAL DEFINITION
//    // ============================================================================
//
//    struct ParameterDefinition {
//        std::string name;
//        ShaderLib::DescriptorType descriptorType;
//        Material::ParamValue value;
//
//        // For textures
//        AssetLib::SamplerDescription samplerDesc;
//
//        // Cached binding from shader (optional, for validation)
//        std::optional<uint32_t> expectedBinding;
//
//        ParameterDefinition() = default;
//
//        // Buffer parameter constructor
//        ParameterDefinition(
//            const std::string& paramName,
//            const Material::ParamValue& paramValue
//        );
//
//        // Texture parameter constructor
//        ParameterDefinition(
//            const std::string& paramName,
//            const std::string& texturePath,
//            AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB,
//            const AssetLib::SamplerDescription& sampler = AssetLib::GetDefaultSampler()
//        );
//
//        bool isBufferParameter() const;
//        bool isTextureParameter() const;
//        ShaderLib::BaseType getBaseType() const;
//    };
//
//    struct MaterialDefinition {
//        std::string materialName;
//        std::string shaderName;
//        std::vector<ParameterDefinition> parameters;
//        std::string sourceInfo = "MaterialCreator";
//
//        const ParameterDefinition* findParameter(const std::string& name) const;
//        ParameterDefinition* findParameter(const std::string& name);
//        bool hasParameter(const std::string& name) const;
//    };
//
//    // ============================================================================
//    // VALIDATION RESULT
//    // ============================================================================
//
//    struct ValidationResult {
//        bool isValid = true;
//        std::vector<std::string> errors;
//        std::vector<std::string> warnings;
//
//        void addError(const std::string& error);
//        void addWarning(const std::string& warning);
//        std::string getSummary() const;
//    };
//
//    // ============================================================================
//    // CONSTRUCTION
//    // ============================================================================
//
//    MaterialCreator();
//    ~MaterialCreator();
//
//    // ============================================================================
//    // MATERIAL CREATION
//    // ============================================================================
//
//    /**
//     * Create and save a material to file
//     *
//     * @param definition Material definition with parameters
//     * @param outputPath Output file path (should end with .amat)
//     * @param compression Compression type to use
//     * @param compressionLevel Compression level (1-9)
//     * @return true if successful, false otherwise
//     */
//    bool createMaterial(
//        const MaterialDefinition& definition,
//        const std::string& outputPath,
//        AssetLib::CompressionType compression = AssetLib::CompressionType::LZ4,
//        int compressionLevel = 1
//    );
//
//    // ============================================================================
//    // VALIDATION
//    // ============================================================================
//
//    /**
//     * Validate material definition without creating file
//     *
//     * @param definition Material definition to validate
//     * @param shaderMetadata Optional shader metadata for advanced validation
//     * @return Validation result with errors/warnings
//     */
//    ValidationResult validateDefinition(
//        const MaterialDefinition& definition,
//        const ShaderLib::ShaderMetadata* shaderMetadata = nullptr
//    ) const;
//
//    // ============================================================================
//    // SHADER-BASED GENERATION
//    // ============================================================================
//
//    /**
//     * Generate parameter definitions from shader metadata
//     * Creates parameters with default values for all shader uniforms and textures
//     *
//     * @param metadata Shader metadata to extract parameters from
//     * @param includeGlobalUBO Include parameters from global UBO (usually false for materials)
//     * @param includeObjectUBO Include parameters from object UBO (usually false for materials)
//     * @return Vector of generated parameter definitions
//     */
//    static std::vector<ParameterDefinition> generateParametersFromShader(
//        const ShaderLib::ShaderMetadata& metadata,
//        bool includeGlobalUBO = false,
//        bool includeObjectUBO = false
//    );
//
//    // ============================================================================
//    // HELPER FACTORIES
//    // ============================================================================
//
//    // Create buffer parameters with default values
//    static ParameterDefinition createFloatParam(const std::string& name, float value = 0.0f);
//    static ParameterDefinition createVec2Param(const std::string& name, const glm::vec2& value = glm::vec2(0.0f));
//    static ParameterDefinition createVec3Param(const std::string& name, const glm::vec3& value = glm::vec3(0.0f));
//    static ParameterDefinition createVec4Param(const std::string& name, const glm::vec4& value = glm::vec4(0.0f));
//    static ParameterDefinition createIntParam(const std::string& name, int32_t value = 0);
//    static ParameterDefinition createBoolParam(const std::string& name, bool value = false);
//    static ParameterDefinition createMat4Param(const std::string& name, const glm::mat4& value = glm::mat4(1.0f));
//
//    // Create texture parameter
//    static ParameterDefinition createTextureParam(
//        const std::string& name,
//        const std::string& texturePath = "",
//        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB,
//        const AssetLib::SamplerDescription& sampler = AssetLib::GetDefaultSampler()
//    );
//
//    // Create composite type parameters
//    static ParameterDefinition createStructParam(
//        const std::string& name,
//        std::shared_ptr<ShaderLib::ShaderStructInstance> structInstance
//    );
//
//    static ParameterDefinition createArrayParam(
//        const std::string& name,
//        std::shared_ptr<ShaderLib::ShaderArrayInstance> arrayInstance
//    );
//
//    // ============================================================================
//    // UTILITY
//    // ============================================================================
//
//    static bool materialExists(const std::string& path);
//    static std::string generateDefaultPath(const std::string& materialName);
//
//private:
//    // ============================================================================
//    // CONVERSION & SERIALIZATION
//    // ============================================================================
//
//    /**
//     * Convert MaterialCreator::ParameterDefinition to AssetLib::ParameterValue
//     */
//    AssetLib::ParameterValue convertToAssetParameter(
//        const ParameterDefinition& paramDef
//    ) const;
//
//    /**
//     * Serialize Material::ParamValue to binary data
//     * Handles base types, composite types, and texture paths
//     */
//    std::vector<uint8_t> serializeParameterValue(
//        const Material::ParamValue& value
//    ) const;
//
//    /**
//     * Calculate size of serialized parameter
//     */
//    size_t getParameterSize(const Material::ParamValue& value) const;
//
//    /**
//     * Serialize composite type definition to JSON string
//     */
//    std::string serializeCompositeDefinition(
//        std::shared_ptr<const ShaderLib::CompositeTypeDefinition> composite
//    ) const;
//
//    // ============================================================================
//    // VALIDATION HELPERS
//    // ============================================================================
//
//    bool validateParameter(
//        const ParameterDefinition& param,
//        ValidationResult& result
//    ) const;
//
//    bool validateAgainstShader(
//        const MaterialDefinition& definition,
//        const ShaderLib::ShaderMetadata& metadata,
//        ValidationResult& result
//    ) const;
//
//    bool isValueTypeCompatible(
//        ShaderLib::BaseType baseType,
//        const Material::ParamValue& value
//    ) const;
//
//    // ============================================================================
//    // DEFAULT VALUE GENERATION
//    // ============================================================================
//
//    static Material::ParamValue createDefaultValue(ShaderLib::BaseType type);
//    static Material::ParamValue createDefaultValue(
//        std::shared_ptr<const ShaderLib::CompositeTypeDefinition> composite
//    );
//};