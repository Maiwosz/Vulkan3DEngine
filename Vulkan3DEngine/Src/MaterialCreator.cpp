#include "MaterialCreator.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cstring>
#include <spdlog/spdlog.h>

MaterialCreator::MaterialCreator() {
}

MaterialCreator::~MaterialCreator() {
}

bool MaterialCreator::createMaterial(
    const MaterialDefinition& definition,
    const std::string& outputPath,
    AssetLib::CompressionType compression,
    int compressionLevel
) {
    try {
        // Walidacja definicji
        std::string errorMessage;
        if (!validateDefinition(definition, errorMessage)) {
            SPDLOG_ERROR("MaterialCreator: Invalid material definition: {}", errorMessage);
            return false;
        }

        // Sprawdzenie czy katalog docelowy istnieje, jeśli nie - utworzenie
        std::filesystem::path filePath(outputPath);
        std::filesystem::path dirPath = filePath.parent_path();

        if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
            if (!std::filesystem::create_directories(dirPath)) {
                SPDLOG_ERROR("MaterialCreator: Failed to create directory: {}", dirPath.string());
                return false;
            }
        }

        // Przygotowanie danych parametrów
        std::vector<AssetLib::MaterialParameter> assetParameters;
        std::vector<uint8_t> parameterData;
        uint32_t currentOffset = 0;

        assetParameters.reserve(definition.parameters.size());

        for (const auto& paramDef : definition.parameters) {
            // Serializacja wartości parametru
            std::vector<uint8_t> paramData = serializeParameterValue(paramDef.defaultValue);

            // Tworzenie AssetLib::MaterialParameter
            AssetLib::MaterialParameter assetParam = convertToAssetParameter(
                paramDef,
                currentOffset,
                static_cast<uint32_t>(paramData.size())
            );

            assetParameters.push_back(assetParam);

            // Dodanie danych do bufora
            parameterData.insert(parameterData.end(), paramData.begin(), paramData.end());
            currentOffset += static_cast<uint32_t>(paramData.size());
        }

        // Tworzenie MaterialInfo
        AssetLib::MaterialInfo materialInfo{};

        // Kopiowanie nazwy shadera (z zabezpieczeniem przed overflow)
        std::string shaderName = definition.shaderName;
        size_t copySize = std::min(shaderName.size(), materialInfo.shaderName.size() - 1);
        std::copy_n(shaderName.c_str(), copySize, materialInfo.shaderName.data());
        materialInfo.shaderName[copySize] = '\0'; // Zapewnienie null-termination

        materialInfo.parameterCount = static_cast<uint32_t>(assetParameters.size());
        materialInfo.dataSize = static_cast<uint32_t>(parameterData.size());

        // Utworzenie AssetData
        AssetLib::AssetData assetData = AssetLib::WriteMaterial(
            definition.sourceInfo.empty() ? "MaterialCreator" : definition.sourceInfo,
            materialInfo,
            assetParameters,
            parameterData,
            compression,
            compressionLevel
        );

        // Zapisanie do pliku
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

AssetLib::SamplerDescription MaterialCreator::createDefaultSampler() {
    AssetLib::SamplerDescription sampler{};
    sampler.magFilter = AssetLib::SamplerDescription::Filter::Linear;
    sampler.minFilter = AssetLib::SamplerDescription::Filter::Linear;
    sampler.addressModeU = AssetLib::SamplerDescription::AddressMode::Repeat;
    sampler.addressModeV = AssetLib::SamplerDescription::AddressMode::Repeat;
    sampler.addressModeW = AssetLib::SamplerDescription::AddressMode::Repeat;
    sampler.anisotropy = 16.0f;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1000.0f;
    sampler.colorSpace = AssetLib::ColorSpace::SRGB;
    return sampler;
}

AssetLib::SamplerDescription MaterialCreator::createLinearSampler() {
    auto sampler = createDefaultSampler();
    sampler.magFilter = AssetLib::SamplerDescription::Filter::Linear;
    sampler.minFilter = AssetLib::SamplerDescription::Filter::Linear;
    return sampler;
}

AssetLib::SamplerDescription MaterialCreator::createNearestSampler() {
    auto sampler = createDefaultSampler();
    sampler.magFilter = AssetLib::SamplerDescription::Filter::Nearest;
    sampler.minFilter = AssetLib::SamplerDescription::Filter::Nearest;
    return sampler;
}

AssetLib::SamplerDescription MaterialCreator::createClampedSampler() {
    auto sampler = createDefaultSampler();
    sampler.addressModeU = AssetLib::SamplerDescription::AddressMode::ClampToEdge;
    sampler.addressModeV = AssetLib::SamplerDescription::AddressMode::ClampToEdge;
    sampler.addressModeW = AssetLib::SamplerDescription::AddressMode::ClampToEdge;
    return sampler;
}

MaterialCreator::ParameterDefinition MaterialCreator::createFloatParam(const std::string& name, float defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Float, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec2Param(const std::string& name, const glm::vec2& defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Vec2, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec3Param(const std::string& name, const glm::vec3& defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Vec3, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec4Param(const std::string& name, const glm::vec4& defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Vec4, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createIntParam(const std::string& name, int32_t defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Int, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createBoolParam(const std::string& name, bool defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Bool, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createMat4Param(const std::string& name, const glm::mat4& defaultValue) {
    return ParameterDefinition(name, ShaderLib::UniformType::Mat4, defaultValue);
}

MaterialCreator::ParameterDefinition MaterialCreator::createTextureParam(
    const std::string& name,
    const std::string& texturePath,
    AssetLib::ColorSpace colorSpace,
    const AssetLib::SamplerDescription& sampler
) {
    return ParameterDefinition(name, texturePath, sampler, colorSpace);
}

bool MaterialCreator::validateDefinition(const MaterialDefinition& definition, std::string& errorMessage) const {
    if (definition.materialName.empty()) {
        errorMessage = "Material name cannot be empty";
        return false;
    }

    if (definition.shaderName.empty()) {
        errorMessage = "Shader name cannot be empty";
        return false;
    }

    // Sprawdzenie czy nazwa shadera nie jest za długa
    if (definition.shaderName.size() >= 32) {
        errorMessage = "Shader name too long (max 31 characters)";
        return false;
    }

    // Walidacja parametrów
    for (const auto& param : definition.parameters) {
        if (!validateParameter(param, errorMessage)) {
            return false;
        }
    }

    return true;
}

bool MaterialCreator::materialExists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::vector<uint8_t> MaterialCreator::serializeParameterValue(const Material::ParamValue& value) const {
    std::vector<uint8_t> data;

    std::visit([&data](const auto& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, Material::TextureParam>) {
            // Dla tekstur zapisujemy ścieżkę jako string
            std::string path = val.handle.filename;
            data.resize(path.size() + 1); // +1 dla null terminatora
            std::copy(path.c_str(), path.c_str() + path.size() + 1, data.data());
        }
        else if constexpr (std::is_same_v<T, bool>) {
            // Bool konwertujemy na uint32_t (zgodnie z konwencją w MaterialManager)
            uint32_t boolValue = val ? 1u : 0u;
            data.resize(sizeof(uint32_t));
            std::memcpy(data.data(), &boolValue, sizeof(uint32_t));
        }
        else {
            // Dla innych typów kopiujemy bezpośrednio
            data.resize(sizeof(T));
            std::memcpy(data.data(), &val, sizeof(T));
        }
        }, value);

    return data;
}

size_t MaterialCreator::getParameterSize(const Material::ParamValue& value) const {
    return std::visit([](const auto& val) -> size_t {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, Material::TextureParam>) {
            return val.handle.filename.size() + 1; // +1 dla null terminatora
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return sizeof(uint32_t); // Bool jako uint32_t
        }
        else {
            return sizeof(T);
        }
        }, value);
}

AssetLib::MaterialParameter MaterialCreator::convertToAssetParameter(
    const ParameterDefinition& paramDef,
    uint32_t dataOffset,
    uint32_t dataSize
) const {
    AssetLib::MaterialParameter assetParam{};

    // Kopiowanie nazwy parametru (z zabezpieczeniem przed overflow)
    size_t copySize = std::min(paramDef.name.size(), assetParam.name.size() - 1);
    std::copy_n(paramDef.name.c_str(), copySize, assetParam.name.data());
    assetParam.name[copySize] = '\0'; // Zapewnienie null-termination

    assetParam.descriptorType = paramDef.descriptorType;
    assetParam.uniformType = paramDef.uniformType;
    assetParam.arraySize = paramDef.arraySize;
    assetParam.samplerDesc = paramDef.samplerDesc;
    assetParam.dataOffset = dataOffset;
    assetParam.dataSize = dataSize;

    return assetParam;
}

bool MaterialCreator::validateParameter(const ParameterDefinition& param, std::string& errorMessage) const {
    if (param.name.empty()) {
        errorMessage = "Parameter name cannot be empty";
        return false;
    }

    if (param.name.size() >= 32) {
        errorMessage = "Parameter name '" + param.name + "' too long (max 31 characters)";
        return false;
    }

    // Sprawdzenie zgodności typu uniform z wartością
    if (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
        if (!isValueTypeCompatible(param.uniformType, param.defaultValue)) {
            errorMessage = "Parameter '" + param.name + "' value type doesn't match uniform type";
            return false;
        }
    }

    return true;
}

bool MaterialCreator::isValueTypeCompatible(ShaderLib::UniformType uniformType, const Material::ParamValue& value) const {
    switch (uniformType) {
    case ShaderLib::UniformType::Bool:
        return std::holds_alternative<bool>(value);
    case ShaderLib::UniformType::Float:
        return std::holds_alternative<float>(value);
    case ShaderLib::UniformType::Vec2:
        return std::holds_alternative<glm::vec2>(value);
    case ShaderLib::UniformType::Vec3:
        return std::holds_alternative<glm::vec3>(value);
    case ShaderLib::UniformType::Vec4:
        return std::holds_alternative<glm::vec4>(value);
    case ShaderLib::UniformType::Int:
        return std::holds_alternative<int32_t>(value);
    case ShaderLib::UniformType::IVec2:
        return std::holds_alternative<glm::ivec2>(value);
    case ShaderLib::UniformType::IVec3:
        return std::holds_alternative<glm::ivec3>(value);
    case ShaderLib::UniformType::IVec4:
        return std::holds_alternative<glm::ivec4>(value);
    case ShaderLib::UniformType::UInt:
        return std::holds_alternative<uint32_t>(value);
    case ShaderLib::UniformType::UVec2:
        return std::holds_alternative<glm::uvec2>(value);
    case ShaderLib::UniformType::UVec3:
        return std::holds_alternative<glm::uvec3>(value);
    case ShaderLib::UniformType::UVec4:
        return std::holds_alternative<glm::uvec4>(value);
    case ShaderLib::UniformType::Mat2:
        return std::holds_alternative<glm::mat2>(value);
    case ShaderLib::UniformType::Mat3:
        return std::holds_alternative<glm::mat3>(value);
    case ShaderLib::UniformType::Mat4:
        return std::holds_alternative<glm::mat4>(value);
    default:
        return false;
    }
}

// Implementacja funkcji pomocniczych
namespace MaterialCreatorUtils {
    MaterialCreator::MaterialDefinition createBasicMaterial(
        const std::string& name,
        const std::string& shader,
        const std::vector<std::pair<std::string, std::string>>& textures
    ) {
        MaterialCreator::MaterialDefinition definition;
        definition.materialName = name;
        definition.shaderName = shader;
        definition.sourceInfo = "MaterialCreatorUtils::createBasicMaterial";

        // Dodanie podstawowych parametrów
        definition.parameters.push_back(
            MaterialCreator::createVec4Param("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
        );

        // Dodanie tekstur
        for (const auto& [paramName, texturePath] : textures) {
            definition.parameters.push_back(
                MaterialCreator::createTextureParam(paramName, texturePath)
            );
        }

        return definition;
    }

    MaterialCreator::MaterialDefinition createPBRMaterial(
        const std::string& name,
        const std::string& shader,
        const std::string& albedoTexture,
        const std::string& normalTexture,
        const std::string& metallicRoughnessTexture,
        const glm::vec3& albedoColor,
        float metallic,
        float roughness
    ) {
        MaterialCreator::MaterialDefinition definition;
        definition.materialName = name;
        definition.shaderName = shader;
        definition.sourceInfo = "MaterialCreatorUtils::createPBRMaterial";

        // Dodanie parametrów PBR
        definition.parameters.push_back(
            MaterialCreator::createVec3Param("albedoColor", albedoColor)
        );
        definition.parameters.push_back(
            MaterialCreator::createFloatParam("metallic", metallic)
        );
        definition.parameters.push_back(
            MaterialCreator::createFloatParam("roughness", roughness)
        );

        // Dodanie tekstur jeśli podano
        if (!albedoTexture.empty()) {
            definition.parameters.push_back(
                MaterialCreator::createTextureParam("albedoTexture", albedoTexture, AssetLib::ColorSpace::SRGB)
            );
        }

        if (!normalTexture.empty()) {
            definition.parameters.push_back(
                MaterialCreator::createTextureParam("normalTexture", normalTexture, AssetLib::ColorSpace::Linear)
            );
        }

        if (!metallicRoughnessTexture.empty()) {
            definition.parameters.push_back(
                MaterialCreator::createTextureParam("metallicRoughnessTexture", metallicRoughnessTexture, AssetLib::ColorSpace::Linear)
            );
        }

        return definition;
    }
}