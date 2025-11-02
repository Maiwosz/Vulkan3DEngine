#include "pch.h"
#include "MaterialSerializer.h"
#include "Serialization.h"
#include "TypeSerializationTable.h"
#include "ValueSerialization.h"
#include <algorithm>
#include <cstring>
#include "AssetLib.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"

namespace AssetLib {

    namespace {
        // Helper to safely copy string to fixed-size array
        template<size_t N>
        void CopyToFixedArray(std::array<char, N>& dest, const std::string& src) {
            std::fill(dest.begin(), dest.end(), '\0');
            std::copy_n(src.c_str(), std::min(src.size(), N - 1), dest.data());
        }

        // Helper to safely extract string from fixed-size array
        template<size_t N>
        std::string ExtractFromFixedArray(const std::array<char, N>& src) {
            return std::string(src.data(), strnlen(src.data(), N));
        }
    }

    // ============================================================================
    // JSON SERIALIZATION - MATERIAL DEPENDENCIES
    // ============================================================================

    json MaterialDependenciesToJson(const MaterialDependencies& deps) {
        json texturesArray = json::array();
        for (size_t i = 0; i < deps.textureNames.size(); ++i) {
            texturesArray.push_back({
                {"name", deps.textureNames[i]},
                {"colorSpace", ColorSpaceToString(deps.textureColorSpaces[i])}
                });
        }

        return {
            {"shader", deps.shaderName},
            {"textures", texturesArray}
        };
    }

    MaterialDependencies MaterialDependenciesFromJson(const json& j) {
        MaterialDependencies deps;

        deps.shaderName = j.at("shader").get<std::string>();

        if (j.contains("textures") && j["textures"].is_array()) {
            for (const auto& tex : j["textures"]) {
                deps.textureNames.push_back(tex.at("name").get<std::string>());
                deps.textureColorSpaces.push_back(
                    StringToColorSpace(tex.at("colorSpace").get<std::string>())
                );
            }
        }

        return deps;
    }

    // ============================================================================
    // JSON SERIALIZATION - SAMPLER DESCRIPTION
    // ============================================================================

    json SamplerDescriptionToJson(const SamplerDescription& sampler) {
        return {
            {"magFilter", SamplerFilterToString(sampler.magFilter)},
            {"minFilter", SamplerFilterToString(sampler.minFilter)},
            {"addressModeU", AddressModeToString(sampler.addressModeU)},
            {"addressModeV", AddressModeToString(sampler.addressModeV)},
            {"addressModeW", AddressModeToString(sampler.addressModeW)},
            {"anisotropy", sampler.anisotropy},
            {"minLod", sampler.minLod},
            {"maxLod", sampler.maxLod},
            {"colorSpace", ColorSpaceToString(sampler.colorSpace)}
        };
    }

    SamplerDescription SamplerDescriptionFromJson(const json& j) {
        SamplerDescription desc;

        if (j.is_null() || !j.is_object()) {
            return GetDefaultSampler();
        }

        desc.magFilter = StringToSamplerFilter(j.value("magFilter", "Linear"));
        desc.minFilter = StringToSamplerFilter(j.value("minFilter", "Linear"));
        desc.addressModeU = StringToAddressMode(j.value("addressModeU", "Repeat"));
        desc.addressModeV = StringToAddressMode(j.value("addressModeV", "Repeat"));
        desc.addressModeW = StringToAddressMode(j.value("addressModeW", "Repeat"));
        desc.anisotropy = j.value("anisotropy", 1.0f);
        desc.minLod = j.value("minLod", 0.0f);
        desc.maxLod = j.value("maxLod", 1000.0f);
        desc.colorSpace = StringToColorSpace(j.value("colorSpace", "Linear"));

        return desc;
    }

    // ============================================================================
    // JSON SERIALIZATION - PARAMETER VALUE
    // ============================================================================

    json ParameterValueToJson(const ParameterValue& param) {
        json result = {
            {"name", param.name},
            {"descriptorType", ShaderLib::DescriptorTypeToString(param.descriptorType)},
            {"valueType", ParameterValueTypeToString(param.valueType)},
            {"arraySize", param.arraySize}
        };

        // Add sampler for textures/samplers
        const auto& descInfo = ShaderLib::GetDescriptorTypeInfo(param.descriptorType);
        if (descInfo.IsTexture() || param.IsSampler()) {
            result["sampler"] = SamplerDescriptionToJson(param.samplerDesc);
        }

        // Serialize value based on type
        switch (param.valueType) {
        case ParameterValueType::BaseType: {
            if (param.baseType != ShaderLib::BaseType::Unknown) {
                result["baseType"] = ShaderLib::BaseTypeToString(param.baseType);

                const auto& serInfo = ShaderLib::GetSerializationInfo(param.baseType);
                if (serInfo.SupportsJson()) {
                    result["data"] = serInfo.toJson(param.GetBaseValue());
                }
            }
            break;
        }

        case ParameterValueType::Struct:
        case ParameterValueType::Array: {
            auto composite = param.GetComposite();
            result["typeDef"] = composite->GetDefinition()->ToJson();
            result["data"] = composite->ToJson();
            break;
        }

        case ParameterValueType::TexturePath: {
            result["path"] = param.GetTexturePath();
            break;
        }
        }

        return result;
    }

    ParameterValue ParameterValueFromJson(const json& j) {
        ParameterValue param;

        // Nazwa zawsze musi być w JSON
        param.name = j.at("name").get<std::string>();

        param.descriptorType = ShaderLib::StringToDescriptorType(
            j.at("descriptorType").get<std::string>()
        );
        param.valueType = StringToParameterValueType(
            j.at("valueType").get<std::string>()
        );
        param.arraySize = j.value("arraySize", 1u);

        // Parse sampler if present
        if (j.contains("sampler")) {
            param.samplerDesc = SamplerDescriptionFromJson(j["sampler"]);
        }
        else {
            param.samplerDesc = GetDefaultSampler();
        }

        // Deserialize value based on type
        switch (param.valueType) {
        case ParameterValueType::BaseType: {
            if (j.contains("baseType")) {
                param.baseType = ShaderLib::StringToBaseType(
                    j.at("baseType").get<std::string>()
                );

                if (j.contains("data")) {
                    const auto& serInfo = ShaderLib::GetSerializationInfo(param.baseType);
                    if (serInfo.SupportsJson()) {
                        param.value = serInfo.fromJson(j["data"]);
                    }
                }
            }
            break;
        }

        case ParameterValueType::Struct: {
            if (!j.contains("typeDef") || !j.contains("data")) {
                throw std::runtime_error("Struct parameter missing typeDef or data");
            }

            auto compositeDef = ShaderLib::CompositeTypeDefinition::FromJson(j["typeDef"]);
            auto instance = compositeDef->CreateInstance();

            if (!instance->FromJson(j["data"])) {
                throw std::runtime_error("Failed to deserialize struct data");
            }

            auto structInstance = std::dynamic_pointer_cast<ShaderLib::ShaderStructInstance>(instance);
            if (!structInstance) {
                throw std::runtime_error("Failed to cast to ShaderStructInstance");
            }
            param.value = structInstance;
            break;
        }

        case ParameterValueType::Array: {
            if (!j.contains("typeDef") || !j.contains("data")) {
                throw std::runtime_error("Array parameter missing typeDef or data");
            }

            auto compositeDef = ShaderLib::CompositeTypeDefinition::FromJson(j["typeDef"]);
            auto instance = compositeDef->CreateInstance();

            if (!instance->FromJson(j["data"])) {
                throw std::runtime_error("Failed to deserialize array data");
            }

            auto arrayInstance = std::dynamic_pointer_cast<ShaderLib::ShaderArrayInstance>(instance);
            if (!arrayInstance) {
                throw std::runtime_error("Failed to cast to ShaderArrayInstance");
            }
            param.value = arrayInstance;
            break;
        }

        case ParameterValueType::TexturePath: {
            param.value = j.at("path").get<std::string>();
            break;
        }
        }

        return param;
    }

    // ============================================================================
    // JSON SERIALIZATION - MATERIAL
    // ============================================================================

    json MaterialToJson(const MaterialDefinition& material) {
        json result = {
            {"shader", material.shaderName},
            {"parameters", json::array()}
        };

        // Serialize jako array, każdy parametr to osobny obiekt
        for (const auto& param : material.parameters) {
            result["parameters"].push_back(ParameterValueToJson(param));
        }

        return result;
    }

    MaterialDefinition MaterialFromJson(const json& j) {
        MaterialDefinition material;

        material.shaderName = j.at("shader").get<std::string>();

        // Teraz parameters to ARRAY, nie object
        if (j.contains("parameters") && j["parameters"].is_array()) {
            for (const auto& paramJson : j["parameters"]) {
                material.parameters.push_back(ParameterValueFromJson(paramJson));
            }
        }

        if (!material.Validate()) {
            throw std::runtime_error("Invalid material structure");
        }

        return material;
    }

    // ============================================================================
    // BINARY FORMAT HELPERS
    // ============================================================================

    json MaterialParameterToJson(const MaterialParameter& param) {
        json j = {
            {"name", ExtractFromFixedArray(param.name)},
            {"descriptorType", static_cast<int>(param.descriptorType)},
            {"valueType", static_cast<int>(param.valueType)},
            {"baseType", static_cast<int>(param.baseType)},
            {"arraySize", param.arraySize},
            {"dataOffset", param.dataOffset},
            {"dataSize", param.dataSize},
            {"compositeDefOffset", param.compositeDefOffset},
            {"compositeDefSize", param.compositeDefSize}
        };

        // Add sampler for relevant types
        const auto& descInfo = ShaderLib::GetDescriptorTypeInfo(param.descriptorType);
        if (descInfo.IsTexture() || param.descriptorType == ShaderLib::DescriptorType::Sampler) {
            j["sampler"] = {
                {"magFilter", static_cast<int>(param.samplerDesc.magFilter)},
                {"minFilter", static_cast<int>(param.samplerDesc.minFilter)},
                {"addressModeU", static_cast<int>(param.samplerDesc.addressModeU)},
                {"addressModeV", static_cast<int>(param.samplerDesc.addressModeV)},
                {"addressModeW", static_cast<int>(param.samplerDesc.addressModeW)},
                {"anisotropy", param.samplerDesc.anisotropy},
                {"minLod", param.samplerDesc.minLod},
                {"maxLod", param.samplerDesc.maxLod},
                {"colorSpace", static_cast<int>(param.samplerDesc.colorSpace)}
            };
        }

        return j;
    }

    MaterialParameter MaterialParameterFromJson(const json& j) {
        MaterialParameter param{};

        CopyToFixedArray(param.name, j.at("name").get<std::string>());
        param.descriptorType = static_cast<ShaderLib::DescriptorType>(j.at("descriptorType").get<int>());
        param.valueType = static_cast<ParameterValueType>(j.at("valueType").get<int>());
        param.baseType = static_cast<ShaderLib::BaseType>(j.at("baseType").get<int>());
        param.arraySize = j.at("arraySize").get<uint32_t>();
        param.dataOffset = j.at("dataOffset").get<uint32_t>();
        param.dataSize = j.at("dataSize").get<uint32_t>();
        param.compositeDefOffset = j.value("compositeDefOffset", 0u);
        param.compositeDefSize = j.value("compositeDefSize", 0u);

        if (j.contains("sampler")) {
            const auto& s = j["sampler"];
            param.samplerDesc.magFilter = static_cast<SamplerDescription::Filter>(s.at("magFilter").get<int>());
            param.samplerDesc.minFilter = static_cast<SamplerDescription::Filter>(s.at("minFilter").get<int>());
            param.samplerDesc.addressModeU = static_cast<SamplerDescription::AddressMode>(s.at("addressModeU").get<int>());
            param.samplerDesc.addressModeV = static_cast<SamplerDescription::AddressMode>(s.at("addressModeV").get<int>());
            param.samplerDesc.addressModeW = static_cast<SamplerDescription::AddressMode>(s.at("addressModeW").get<int>());
            param.samplerDesc.anisotropy = s.at("anisotropy").get<float>();
            param.samplerDesc.minLod = s.at("minLod").get<float>();
            param.samplerDesc.maxLod = s.at("maxLod").get<float>();
            param.samplerDesc.colorSpace = static_cast<ColorSpace>(s.value("colorSpace", 0));
        }
        else {
            param.samplerDesc = GetDefaultSampler();
        }

        return param;
    }

    json MaterialInfoToJson(const MaterialInfo& info) {
        return {
            {"shaderName", ExtractFromFixedArray(info.shaderName)},
            {"parameterCount", info.parameterCount},
            {"dataSize", info.dataSize},
            {"compositeDefsSize", info.compositeDefsSize}
        };
    }

    MaterialInfo MaterialInfoFromJson(const json& j) {
        MaterialInfo info{};
        CopyToFixedArray(info.shaderName, j.at("shaderName").get<std::string>());
        info.parameterCount = j.at("parameterCount").get<uint32_t>();
        info.dataSize = j.at("dataSize").get<uint32_t>();
        info.compositeDefsSize = j.at("compositeDefsSize").get<uint32_t>();
        return info;
    }

    // ============================================================================
    // HIGH-LEVEL TO LOW-LEVEL CONVERSION
    // ============================================================================

    MaterialData MaterialToData(const MaterialDefinition& material) {
        if (!material.Validate()) {
            throw std::runtime_error("Invalid material");
        }

        MaterialData data;
        std::vector<uint8_t> paramData;
        json compositeDefsJson = json::object();

        // Set shader name
        CopyToFixedArray(data.info.shaderName, material.shaderName);

        // Process each parameter
        for (const auto& param : material.parameters) {
            MaterialParameter binParam{};

            CopyToFixedArray(binParam.name, param.name);
            binParam.descriptorType = param.descriptorType;
            binParam.valueType = param.valueType;
            binParam.baseType = param.baseType;
            binParam.arraySize = param.arraySize;
            binParam.samplerDesc = param.samplerDesc;
            binParam.dataOffset = static_cast<uint32_t>(paramData.size());

            // Serialize value to binary
            switch (param.valueType) {
            case ParameterValueType::BaseType: {
                if (param.baseType != ShaderLib::BaseType::Unknown) {
                    size_t sizeBefore = paramData.size();
                    if (!ShaderLib::WriteBaseTypeToBuffer(param.baseType, paramData, param.GetBaseValue())) {
                        throw std::runtime_error("Failed to serialize base type: " + param.name);
                    }
                    binParam.dataSize = static_cast<uint32_t>(paramData.size() - sizeBefore);
                }
                else {
                    binParam.dataSize = 0;
                }
                binParam.compositeDefOffset = 0;
                binParam.compositeDefSize = 0;
                break;
            }

            case ParameterValueType::Struct:
            case ParameterValueType::Array: {
                auto composite = param.GetComposite();

                // Store composite definition
                binParam.compositeDefOffset = static_cast<uint32_t>(compositeDefsJson.size());
                json def = composite->GetDefinition()->ToJson();  // ✅ NOWA METODA
                compositeDefsJson[param.name] = def;
                binParam.compositeDefSize = static_cast<uint32_t>(def.dump().size());

                // Store binary data
                size_t sizeBefore = paramData.size();
                const auto& buffer = composite->GetRawBuffer();
                paramData.insert(paramData.end(), buffer.begin(), buffer.end());
                binParam.dataSize = static_cast<uint32_t>(paramData.size() - sizeBefore);
                break;
            }

            case ParameterValueType::TexturePath: {
                const std::string& path = param.GetTexturePath();
                paramData.insert(paramData.end(), path.begin(), path.end());
                paramData.push_back('\0');
                binParam.dataSize = static_cast<uint32_t>(path.length() + 1);
                binParam.compositeDefOffset = 0;
                binParam.compositeDefSize = 0;
                break;
            }
            }

            data.parameters.push_back(binParam);
        }

        // Finalize
        data.info.parameterCount = static_cast<uint32_t>(data.parameters.size());
        data.info.dataSize = static_cast<uint32_t>(paramData.size());
        data.parameterData = std::move(paramData);
        data.compositeDefinitions = compositeDefsJson.dump();
        data.info.compositeDefsSize = static_cast<uint32_t>(data.compositeDefinitions.size());

        return data;
    }

    // ============================================================================
    // LOW-LEVEL TO HIGH-LEVEL CONVERSION
    // ============================================================================

    MaterialDefinition DataToMaterial(const MaterialData& data) {
        MaterialDefinition material;
        material.shaderName = ExtractFromFixedArray(data.info.shaderName);

        // Parse composite definitions
        json compositeDefsJson = data.compositeDefinitions.empty() ?
            json::object() : json::parse(data.compositeDefinitions);

        // Process each parameter
        for (const auto& binParam : data.parameters) {
            ParameterValue param;

            param.name = ExtractFromFixedArray(binParam.name);
            param.descriptorType = binParam.descriptorType;
            param.valueType = binParam.valueType;
            param.baseType = binParam.baseType;
            param.arraySize = binParam.arraySize;
            param.samplerDesc = binParam.samplerDesc;

            // Deserialize value from binary
            switch (param.valueType) {
            case ParameterValueType::BaseType: {
                if (binParam.baseType != ShaderLib::BaseType::Unknown && binParam.dataSize > 0) {
                    const void* dataPtr = data.parameterData.data() + binParam.dataOffset;
                    // Store directly in BufferValue
                    param.value = ShaderLib::ReadBaseTypeFromBuffer(binParam.baseType, dataPtr);
                }
                break;
            }

            case ParameterValueType::Struct: {
                if (!compositeDefsJson.contains(param.name)) {
                    throw std::runtime_error("Missing composite definition for: " + param.name);
                }

                auto compositeDef = ShaderLib::CompositeTypeDefinition::FromJson(
                    compositeDefsJson[param.name]
                );
                auto instance = compositeDef->CreateInstance();

                const void* dataPtr = data.parameterData.data() + binParam.dataOffset;
                if (!instance->ReadFromBuffer(dataPtr)) {
                    throw std::runtime_error("Failed to read struct data: " + param.name);
                }

                auto structInstance = std::dynamic_pointer_cast<ShaderLib::ShaderStructInstance>(instance);
                if (!structInstance) {
                    throw std::runtime_error("Failed to cast to ShaderStructInstance");
                }
                param.value = structInstance;
                break;
            }

            case ParameterValueType::Array: {
                if (!compositeDefsJson.contains(param.name)) {
                    throw std::runtime_error("Missing composite definition for: " + param.name);
                }

                auto compositeDef = ShaderLib::CompositeTypeDefinition::FromJson(
                    compositeDefsJson[param.name]
                );
                auto instance = compositeDef->CreateInstance();

                const void* dataPtr = data.parameterData.data() + binParam.dataOffset;
                if (!instance->ReadFromBuffer(dataPtr)) {
                    throw std::runtime_error("Failed to read array data: " + param.name);
                }

                auto arrayInstance = std::dynamic_pointer_cast<ShaderLib::ShaderArrayInstance>(instance);
                if (!arrayInstance) {
                    throw std::runtime_error("Failed to cast to ShaderArrayInstance");
                }
                param.value = arrayInstance;
                break;
            }

            case ParameterValueType::TexturePath: {
                const char* pathPtr = reinterpret_cast<const char*>(
                    data.parameterData.data() + binParam.dataOffset
                    );
                param.value = std::string(pathPtr, binParam.dataSize - 1); // -1 for null terminator
                break;
            }
            }

            material.parameters.push_back(std::move(param));
        }

        if (!material.Validate()) {
            throw std::runtime_error("Deserialized invalid material");
        }

        return material;
    }

    // ============================================================================
    // HIGH-LEVEL ASSET SERIALIZATION
    // ============================================================================

    AssetData WriteMaterial(
        const std::string& source,
        const MaterialDefinition& material,
        CompressionType compression,
        int compressionLevel)
    {
        // Convert to low-level format
        MaterialData data = MaterialToData(material);

        // Extract dependencies for metadata
        MaterialDependencies deps = material.ExtractDependencies();

        // Create asset
        AssetData asset;
        asset.header.assetType = AssetType::Material;
        asset.header.compression = compression;
        asset.header.decompressedSize = static_cast<uint32_t>(data.parameterData.size());

        // Set metadata
        asset.metadata["source"] = source;
        asset.metadata["material"] = MaterialInfoToJson(data.info);
        asset.metadata["dependencies"] = MaterialDependenciesToJson(deps);

        json paramsJson = json::array();
        for (const auto& param : data.parameters) {
            paramsJson.push_back(MaterialParameterToJson(param));
        }
        asset.metadata["parameters"] = paramsJson;
        asset.metadata["compositeDefinitions"] = data.compositeDefinitions;

        // Compress data
        asset.compressedData = Compress(
            data.parameterData.data(),
            data.parameterData.size(),
            compression,
            compressionLevel
        );

        return asset;
    }

    MaterialDefinition ReadMaterial(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }

        MaterialData data;

        // Deserialize metadata
        data.info = MaterialInfoFromJson(asset.metadata.at("material"));

        for (const auto& paramJson : asset.metadata.at("parameters")) {
            data.parameters.push_back(MaterialParameterFromJson(paramJson));
        }

        data.compositeDefinitions = asset.metadata.value("compositeDefinitions", "");

        // Decompress data
        data.parameterData = Decompress(
            asset.compressedData.data(),
            asset.compressedData.size(),
            asset.header.decompressedSize
        );

        // Convert to high-level format
        return DataToMaterial(data);
    }

    MaterialDependencies ReadMaterialDependencies(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }

        if (!asset.metadata.contains("dependencies")) {
            throw std::runtime_error("Material asset missing dependencies metadata");
        }

        return MaterialDependenciesFromJson(asset.metadata.at("dependencies"));
    }

} // namespace AssetLib