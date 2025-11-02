#include "MaterialCreator.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <MaterialSerializer.h>

// ============================================================================
// PARAMETER DEFINITION CONSTRUCTORS
// ============================================================================

MaterialCreator::ParameterDefinition::ParameterDefinition(
    const std::string& paramName,
    const Material::ParamValue& paramValue
) : name(paramName), value(paramValue) {

    if (std::holds_alternative<Material::TextureParam>(paramValue)) {
        descriptorType = ShaderLib::DescriptorType::Sampler2D;
        samplerDesc = AssetLib::GetDefaultSampler();
    }
    else {
        descriptorType = ShaderLib::DescriptorType::UniformBuffer;
    }
}

MaterialCreator::ParameterDefinition::ParameterDefinition(
    const std::string& paramName,
    const std::string& texturePath,
    AssetLib::ColorSpace colorSpace,
    const AssetLib::SamplerDescription& sampler
) : name(paramName),
descriptorType(ShaderLib::DescriptorType::Sampler2D),
samplerDesc(sampler) {

    Material::TextureParam textureParam;
    textureParam.handle = AssetHandle(AssetType::Texture, texturePath);
    textureParam.colorSpace = colorSpace;
    value = textureParam;
}

bool MaterialCreator::ParameterDefinition::isBufferParameter() const {
    return descriptorType == ShaderLib::DescriptorType::UniformBuffer ||
        descriptorType == ShaderLib::DescriptorType::StorageBuffer;
}

bool MaterialCreator::ParameterDefinition::isTextureParameter() const {
    return std::holds_alternative<Material::TextureParam>(value);
}

ShaderLib::BaseType MaterialCreator::ParameterDefinition::getBaseType() const {
    if (auto* bufVal = std::get_if<ShaderLib::BufferValue>(&value)) {
        return ShaderLib::GetBaseTypeFromVariant(*bufVal);
    }
    return ShaderLib::BaseType::Unknown;
}

// ============================================================================
// MATERIAL DEFINITION HELPERS
// ============================================================================

const MaterialCreator::ParameterDefinition*
MaterialCreator::MaterialDefinition::findParameter(const std::string& name) const {
    for (const auto& param : parameters) {
        if (param.name == name) return &param;
    }
    return nullptr;
}

MaterialCreator::ParameterDefinition*
MaterialCreator::MaterialDefinition::findParameter(const std::string& name) {
    for (auto& param : parameters) {
        if (param.name == name) return &param;
    }
    return nullptr;
}

bool MaterialCreator::MaterialDefinition::hasParameter(const std::string& name) const {
    return findParameter(name) != nullptr;
}

// ============================================================================
// VALIDATION RESULT
// ============================================================================

void MaterialCreator::ValidationResult::addError(const std::string& error) {
    errors.push_back(error);
    isValid = false;
}

void MaterialCreator::ValidationResult::addWarning(const std::string& warning) {
    warnings.push_back(warning);
}

std::string MaterialCreator::ValidationResult::getSummary() const {
    std::string summary;

    if (isValid) {
        summary = "Validation passed";
        if (!warnings.empty()) {
            summary += " with " + std::to_string(warnings.size()) + " warning(s)";
        }
    }
    else {
        summary = "Validation failed with " + std::to_string(errors.size()) + " error(s)";
    }

    for (const auto& error : errors) {
        summary += "\n  ERROR: " + error;
    }

    for (const auto& warning : warnings) {
        summary += "\n  WARNING: " + warning;
    }

    return summary;
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

MaterialCreator::MaterialCreator() = default;
MaterialCreator::~MaterialCreator() = default;

// ============================================================================
// MATERIAL CREATION
// ============================================================================

bool MaterialCreator::createMaterial(
    const MaterialDefinition& definition,
    const std::string& outputPath,
    AssetLib::CompressionType compression,
    int compressionLevel
) {
    try {
        // Validate definition
        ValidationResult validation = validateDefinition(definition);
        if (!validation.isValid) {
            SPDLOG_ERROR("MaterialCreator: Validation failed:\n{}", validation.getSummary());
            return false;
        }

        // Log warnings
        for (const auto& warning : validation.warnings) {
            SPDLOG_WARN("MaterialCreator: {}", warning);
        }

        // Create output directory if needed
        std::filesystem::path filePath(outputPath);
        std::filesystem::path dirPath = filePath.parent_path();

        if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
            if (!std::filesystem::create_directories(dirPath)) {
                SPDLOG_ERROR("MaterialCreator: Failed to create directory: {}", dirPath.string());
                return false;
            }
        }

        // Convert to AssetLib format
        AssetLib::MaterialDefinition assetDef;
        assetDef.shaderName = definition.shaderName;

        for (const auto& param : definition.parameters) {
            assetDef.parameters.push_back(convertToAssetParameter(param));
        }

        // Validate AssetLib definition
        if (!assetDef.Validate()) {
            SPDLOG_ERROR("MaterialCreator: AssetLib validation failed");
            return false;
        }

        // Write material using AssetLib
        AssetLib::AssetData assetData = AssetLib::WriteMaterial(
            definition.sourceInfo,
            assetDef,
            compression,
            compressionLevel
        );

        AssetLib::WriteAsset(outputPath, assetData);

        SPDLOG_INFO("MaterialCreator: Successfully created material '{}' at '{}'",
            definition.materialName, outputPath);
        return true;

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialCreator: Exception while creating material '{}': {}",
            definition.materialName, e.what());
        return false;
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

MaterialCreator::ValidationResult MaterialCreator::validateDefinition(
    const MaterialDefinition& definition,
    const ShaderLib::ShaderMetadata* shaderMetadata
) const {
    ValidationResult result;

    // Basic validation
    if (definition.materialName.empty()) {
        result.addError("Material name cannot be empty");
    }

    if (definition.shaderName.empty()) {
        result.addError("Shader name cannot be empty");
    }

    if (definition.shaderName.size() >= 32) {
        result.addError("Shader name too long (max 31 characters)");
    }

    // Validate each parameter
    for (const auto& param : definition.parameters) {
        validateParameter(param, result);
    }

    // Advanced validation with shader metadata
    if (shaderMetadata && result.isValid) {
        validateAgainstShader(definition, *shaderMetadata, result);
    }

    return result;
}

bool MaterialCreator::validateParameter(
    const ParameterDefinition& param,
    ValidationResult& result
) const {
    bool paramValid = true;

    if (param.name.empty()) {
        result.addError("Parameter name cannot be empty");
        paramValid = false;
    }

    if (param.name.size() >= 32) {
        result.addError("Parameter name '" + param.name + "' too long (max 31 characters)");
        paramValid = false;
    }

    // Validate value type compatibility
    if (param.isBufferParameter()) {
        if (!std::holds_alternative<ShaderLib::BufferValue>(param.value)) {
            result.addError("Parameter '" + param.name + "' is marked as buffer but contains texture data");
            paramValid = false;
        }
    }
    else if (param.isTextureParameter()) {
        if (!std::holds_alternative<Material::TextureParam>(param.value)) {
            result.addError("Parameter '" + param.name + "' is marked as texture but contains buffer data");
            paramValid = false;
        }
        else {
            const auto& texParam = std::get<Material::TextureParam>(param.value);
            if (texParam.handle.filename.empty()) {
                result.addWarning("Texture parameter '" + param.name + "' has empty path");
            }
        }
    }

    return paramValid;
}

bool MaterialCreator::validateAgainstShader(
    const MaterialDefinition& definition,
    const ShaderLib::ShaderMetadata& metadata,
    ValidationResult& result
) const {
    bool valid = true;

    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();
    if (!customSet) {
        result.addWarning("Shader has no custom descriptor set");
        return valid;
    }

    // Check that all parameters match shader expectations
    for (const auto& param : definition.parameters) {
        const ShaderLib::DescriptorSlot* slot = customSet->FindSlot(param.name);

        if (!slot) {
            result.addWarning("Parameter '" + param.name + "' not found in shader");
            continue;
        }

        // Validate descriptor type match
        if (slot->type != param.descriptorType) {
            result.addError("Parameter '" + param.name + "' type mismatch: expected " +
                std::to_string(static_cast<int>(slot->type)) + ", got " +
                std::to_string(static_cast<int>(param.descriptorType)));
            valid = false;
        }

        // For buffer parameters, validate against buffer definition
        if (slot->IsBuffer()) {
            const ShaderLib::BufferObject* buffer = customSet->GetBufferByBinding(slot->binding);
            if (buffer) {
                // Find variable in buffer
                bool foundVar = false;
                for (const auto& var : buffer->variables) {
                    if (var.name == param.name) {
                        foundVar = true;

                        // Validate base type
                        ShaderLib::BaseType paramBaseType = param.getBaseType();
                        if (var.IsBase() && paramBaseType != var.baseType) {
                            result.addError("Parameter '" + param.name + "' base type mismatch");
                            valid = false;
                        }
                        break;
                    }
                }

                if (!foundVar) {
                    result.addWarning("Parameter '" + param.name + "' not found in buffer variables");
                }
            }
        }
    }

    return valid;
}

bool MaterialCreator::isValueTypeCompatible(
    ShaderLib::BaseType baseType,
    const Material::ParamValue& value
) const {
    if (std::holds_alternative<Material::TextureParam>(value)) {
        return baseType == ShaderLib::BaseType::Unknown;
    }

    if (!std::holds_alternative<ShaderLib::BufferValue>(value)) {
        return false;
    }

    const auto& bufVal = std::get<ShaderLib::BufferValue>(value);
    ShaderLib::BaseType valueType = ShaderLib::GetBaseTypeFromVariant(bufVal);

    return valueType == baseType;
}

// ============================================================================
// SHADER-BASED GENERATION
// ============================================================================

std::vector<MaterialCreator::ParameterDefinition>
MaterialCreator::generateParametersFromShader(
    const ShaderLib::ShaderMetadata& metadata,
    bool includeGlobalUBO,
    bool includeObjectUBO
) {
    std::vector<ParameterDefinition> parameters;

    // Process custom descriptor set
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();
    if (!customSet) {
        SPDLOG_WARN("MaterialCreator: Shader has no custom descriptor set");
        return parameters;
    }

    // Process buffers in custom set
    for (const auto& [bufferName, buffer] : customSet->buffers) {
        if (buffer.IsUniformBuffer()) {
            // Add parameters for each variable
            for (const auto& variable : buffer.variables) {
                if (variable.IsBase()) {
                    // Create parameter with default value
                    Material::ParamValue defaultValue = createDefaultValue(variable.baseType);
                    ParameterDefinition param(variable.name, defaultValue);
                    param.expectedBinding = customSet->FindSlot(variable.name)->binding;
                    parameters.push_back(param);

                }
                else if (variable.IsComposite()) {
                    // Create composite parameter with default instance
                    Material::ParamValue defaultValue = createDefaultValue(variable.composite);
                    ParameterDefinition param(variable.name, defaultValue);
                    param.expectedBinding = customSet->FindSlot(bufferName)->binding;
                    parameters.push_back(param);
                }
            }
        }
    }

    // Process texture samplers
    for (const auto& slot : customSet->slots) {
        if (slot.type == ShaderLib::DescriptorType::Sampler2D) {
            ParameterDefinition param = createTextureParam(slot.name);
            param.expectedBinding = slot.binding;
            parameters.push_back(param);
        }
    }

    // Optionally include global/object UBOs
    if (includeGlobalUBO && metadata.usesGlobalUBO) {
        // Similar processing for globalUBO...
    }

    if (includeObjectUBO && metadata.usesObjectUBO) {
        // Similar processing for objectUBO...
    }

    return parameters;
}

// ============================================================================
// HELPER FACTORIES
// ============================================================================

MaterialCreator::ParameterDefinition MaterialCreator::createFloatParam(
    const std::string& name, float value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec2Param(
    const std::string& name, const glm::vec2& value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec3Param(
    const std::string& name, const glm::vec3& value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec4Param(
    const std::string& name, const glm::vec4& value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createIntParam(
    const std::string& name, int32_t value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createBoolParam(
    const std::string& name, bool value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createMat4Param(
    const std::string& name, const glm::mat4& value
) {
    ShaderLib::BufferValue bufVal = value;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createTextureParam(
    const std::string& name,
    const std::string& texturePath,
    AssetLib::ColorSpace colorSpace,
    const AssetLib::SamplerDescription& sampler
) {
    return ParameterDefinition(name, texturePath, colorSpace, sampler);
}

MaterialCreator::ParameterDefinition MaterialCreator::createStructParam(
    const std::string& name,
    std::shared_ptr<ShaderLib::ShaderStructInstance> structInstance
) {
    ShaderLib::BufferValue bufVal = structInstance;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createArrayParam(
    const std::string& name,
    std::shared_ptr<ShaderLib::ShaderArrayInstance> arrayInstance
) {
    ShaderLib::BufferValue bufVal = arrayInstance;
    return ParameterDefinition(name, Material::ParamValue{ bufVal });
}

// ============================================================================
// UTILITY
// ============================================================================

bool MaterialCreator::materialExists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::string MaterialCreator::generateDefaultPath(const std::string& materialName) {
    // Implementation depends on your asset system
    return materialName + ".amat";
}

// ============================================================================
// CONVERSION & SERIALIZATION
// ============================================================================

AssetLib::ParameterValue MaterialCreator::convertToAssetParameter(
    const ParameterDefinition& paramDef
) const {
    AssetLib::ParameterValue assetParam;
    assetParam.name = paramDef.name;
    assetParam.descriptorType = paramDef.descriptorType;

    if (std::holds_alternative<Material::TextureParam>(paramDef.value)) {
        // Texture parameter
        const auto& texParam = std::get<Material::TextureParam>(paramDef.value);
        assetParam.valueType = AssetLib::ParameterValueType::TexturePath;
        assetParam.value = texParam.handle.filename;
        assetParam.samplerDesc = paramDef.samplerDesc;

    }
    else if (std::holds_alternative<ShaderLib::BufferValue>(paramDef.value)) {
        // Buffer parameter
        const auto& bufVal = std::get<ShaderLib::BufferValue>(paramDef.value);

        // Check if it's a composite type
        if (auto structInst = std::get_if<std::shared_ptr<ShaderLib::ShaderStructInstance>>(&bufVal)) {
            assetParam.valueType = AssetLib::ParameterValueType::Struct;
            assetParam.value = bufVal;

        }
        else if (auto arrayInst = std::get_if<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(&bufVal)) {
            assetParam.valueType = AssetLib::ParameterValueType::Array;
            assetParam.value = bufVal;

        }
        else {
            // Base type
            assetParam.valueType = AssetLib::ParameterValueType::BaseType;
            assetParam.baseType = ShaderLib::GetBaseTypeFromVariant(bufVal);
            assetParam.value = bufVal;
        }
    }

    return assetParam;
}

// ============================================================================
// DEFAULT VALUE GENERATION
// ============================================================================

Material::ParamValue MaterialCreator::createDefaultValue(ShaderLib::BaseType type) {
    ShaderLib::BufferValue bufVal;

    switch (type) {
    case ShaderLib::BaseType::Float: bufVal = 0.0f; break;
    case ShaderLib::BaseType::Vec2: bufVal = glm::vec2(0.0f); break;
    case ShaderLib::BaseType::Vec3: bufVal = glm::vec3(0.0f); break;
    case ShaderLib::BaseType::Vec4: bufVal = glm::vec4(0.0f); break;
    case ShaderLib::BaseType::Int: bufVal = int32_t(0); break;
    case ShaderLib::BaseType::Bool: bufVal = false; break;
    case ShaderLib::BaseType::Mat4: bufVal = glm::mat4(1.0f); break;
    case ShaderLib::BaseType::Mat3: bufVal = glm::mat3(1.0f); break;
    case ShaderLib::BaseType::Mat2: bufVal = glm::mat2(1.0f); break;
    case ShaderLib::BaseType::UInt: bufVal = uint32_t(0); break;
    case ShaderLib::BaseType::IVec2: bufVal = glm::ivec2(0); break;
    case ShaderLib::BaseType::IVec3: bufVal = glm::ivec3(0); break;
    case ShaderLib::BaseType::IVec4: bufVal = glm::ivec4(0); break;
    case ShaderLib::BaseType::UVec2: bufVal = glm::uvec2(0u); break;
    case ShaderLib::BaseType::UVec3: bufVal = glm::uvec3(0u); break;
    case ShaderLib::BaseType::UVec4: bufVal = glm::uvec4(0u); break;
    case ShaderLib::BaseType::Double: bufVal = 0.0; break;
    case ShaderLib::BaseType::DVec2: bufVal = glm::dvec2(0.0); break;
    case ShaderLib::BaseType::DVec3: bufVal = glm::dvec3(0.0); break;
    case ShaderLib::BaseType::DVec4: bufVal = glm::dvec4(0.0); break;
    default: bufVal = 0.0f; break;
    }

    return Material::ParamValue{ bufVal };
}

Material::ParamValue MaterialCreator::createDefaultValue(
    std::shared_ptr<const ShaderLib::CompositeTypeDefinition> composite
) {
    if (!composite) {
        return Material::ParamValue{ ShaderLib::BufferValue{0.0f} };
    }

    // Create default instance
    auto instance = composite->CreateInstance();
    ShaderLib::BufferValue bufVal;

    if (composite->IsStruct()) {
        bufVal = std::dynamic_pointer_cast<ShaderLib::ShaderStructInstance>(instance);
    }
    else {
        bufVal = std::dynamic_pointer_cast<ShaderLib::ShaderArrayInstance>(instance);
    }

    return Material::ParamValue{ bufVal };
}