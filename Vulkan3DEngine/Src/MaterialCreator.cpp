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
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Float, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec2Param(const std::string& name, const glm::vec2& defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Vec2, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec3Param(const std::string& name, const glm::vec3& defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Vec3, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createVec4Param(const std::string& name, const glm::vec4& defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Vec4, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createIntParam(const std::string& name, int32_t defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Int, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createBoolParam(const std::string& name, bool defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Bool, Material::ParamValue{ bufVal });
}

MaterialCreator::ParameterDefinition MaterialCreator::createMat4Param(const std::string& name, const glm::mat4& defaultValue) {
    ShaderLib::BufferValue bufVal = defaultValue;
    return ParameterDefinition(name, ShaderLib::BaseType::Mat4, Material::ParamValue{ bufVal });
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

    // Check if it's a TextureParam
    if (std::holds_alternative<Material::TextureParam>(value)) {
        const auto& texParam = std::get<Material::TextureParam>(value);
        std::string path = texParam.handle.filename;
        data.resize(path.size() + 1); // +1 dla null terminatora
        std::copy(path.c_str(), path.c_str() + path.size() + 1, data.data());
        return data;
    }

    // It must be a BufferValue
    const auto& bufVal = std::get<ShaderLib::BufferValue>(value);

    std::visit([&data](const auto& val) {
        using T = std::decay_t<decltype(val)>;

        // Skip composite types (shared_ptr<ShaderStruct/ShaderArray>)
        if constexpr (std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderStruct>> ||
            std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderArray>>) {
            // Not supported in material parameters
            return;
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
        }, bufVal);

    return data;
}

size_t MaterialCreator::getParameterSize(const Material::ParamValue& value) const {
    // Check if it's a TextureParam
    if (std::holds_alternative<Material::TextureParam>(value)) {
        const auto& texParam = std::get<Material::TextureParam>(value);
        return texParam.handle.filename.size() + 1; // +1 dla null terminatora
    }

    // It must be a BufferValue
    const auto& bufVal = std::get<ShaderLib::BufferValue>(value);

    return std::visit([](const auto& val) -> size_t {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderStruct>> ||
            std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderArray>>) {
            return 0; // Not supported
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return sizeof(uint32_t); // Bool jako uint32_t
        }
        else {
            return sizeof(T);
        }
        }, bufVal);
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
    assetParam.baseType = paramDef.baseType;
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

    // Sprawdzenie zgodności typu base z wartością
    if (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer ||
        param.descriptorType == ShaderLib::DescriptorType::StorageBuffer) {
        if (!isValueTypeCompatible(param.baseType, param.defaultValue)) {
            errorMessage = "Parameter '" + param.name + "' value type doesn't match base type";
            return false;
        }
    }

    return true;
}

bool MaterialCreator::isValueTypeCompatible(ShaderLib::BaseType baseType, const Material::ParamValue& value) const {
    // Texture parameters don't have base types
    if (std::holds_alternative<Material::TextureParam>(value)) {
        return baseType == ShaderLib::BaseType::Unknown;
    }

    // Must be BufferValue
    if (!std::holds_alternative<ShaderLib::BufferValue>(value)) {
        return false;
    }

    const auto& bufVal = std::get<ShaderLib::BufferValue>(value);

    // Get the base type from the variant
    ShaderLib::BaseType valueType = ShaderLib::GetBaseTypeFromVariant(bufVal);

    return valueType == baseType;
}