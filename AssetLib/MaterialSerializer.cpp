#include "pch.h"
#include "MaterialSerializer.h"
#include "Serialization.h"
#include "TypeSerializationTable.h"
#include <algorithm>
#include <cstring>
#include "AssetLib.h"

namespace AssetLib {

    // ============================================================================
// BUFFER RECONSTRUCTION FROM JSON (Helper functions)
// ============================================================================

    namespace {
        template<size_t N>
        void CopyToArray(std::array<char, N>& dest, const std::string& src) {
            std::fill(dest.begin(), dest.end(), '\0');
            std::copy_n(src.c_str(), std::min(src.size(), N - 1), dest.data());
        }

        template<size_t N>
        std::string ExtractFromArray(const std::array<char, N>& src) {
            return std::string(src.data(), strnlen(src.data(), N));
        }

        // Parse field value - expects {baseType, value} format
        struct FieldSpec {
            ShaderLib::BaseType baseType;
            json value;
            bool isArray = false;
            uint32_t arraySize = 0;
        };

        FieldSpec ParseFieldSpec(const json& fieldJson, const std::string& fieldName) {
            if (!fieldJson.is_object()) {
                throw std::runtime_error(
                    "Field '" + fieldName + "' must be an object with 'baseType' and 'value'"
                );
            }

            if (!fieldJson.contains("baseType")) {
                throw std::runtime_error(
                    "Field '" + fieldName + "' is missing required 'baseType' field"
                );
            }

            if (!fieldJson.contains("value")) {
                throw std::runtime_error(
                    "Field '" + fieldName + "' is missing required 'value' field"
                );
            }

            FieldSpec spec;
            std::string typeStr = fieldJson.at("baseType").get<std::string>();
            spec.baseType = ShaderLib::StringToBaseType(typeStr);

            if (spec.baseType == ShaderLib::BaseType::Unknown) {
                throw std::runtime_error(
                    "Field '" + fieldName + "' has invalid baseType: " + typeStr
                );
            }

            spec.value = fieldJson.at("value");

            // Check if it's an array
            if (spec.value.is_array()) {
                spec.isArray = true;
                spec.arraySize = static_cast<uint32_t>(spec.value.size());

                if (spec.arraySize == 0) {
                    throw std::runtime_error(
                        "Field '" + fieldName + "' has empty array"
                    );
                }
            }

            return spec;
        }

        // Set field value from parsed spec
        void SetFieldValueFromSpec(
            std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
            const std::string& path,
            const json& value,
            ShaderLib::BaseType expectedType)
        {
            const auto& info = ShaderLib::GetSerializationInfo(expectedType);
            (*buffer)[path] = info.fromJson(value);
        }

        // Recursively build StructureDefinition from field specs
        void BuildStructureFromFields(
            std::shared_ptr<ShaderLib::StructureDefinition> structDef,
            const json& fieldsJson)
        {
            for (const auto& [fieldName, fieldValue] : fieldsJson.items()) {
                // Check if it's a nested structure (object without baseType/value)
                if (fieldValue.is_object() &&
                    !fieldValue.contains("baseType") &&
                    !fieldValue.contains("value")) {

                    // Nested structure - create it and recurse
                    auto nestedStruct = ShaderLib::MakeStruct(fieldName);

                    BuildStructureFromFields(nestedStruct, fieldValue);

                    structDef->AddField(fieldName, nestedStruct);
                }
                else {
                    // Base type field - must have baseType and value
                    FieldSpec spec = ParseFieldSpec(fieldValue, fieldName);

                    if (spec.isArray) {
                        structDef->AddField(fieldName, spec.baseType, spec.arraySize);
                    }
                    else {
                        structDef->AddField(fieldName, spec.baseType);
                    }
                }
            }
        }

        // Recursively fill buffer with values
        void FillBufferFromFields(
            std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
            const json& fieldsJson,
            const std::string& parentPath = "")
        {
            for (const auto& [fieldName, fieldValue] : fieldsJson.items()) {
                std::string fieldPath = parentPath.empty()
                    ? fieldName
                    : parentPath + "." + fieldName;

                // Check if nested structure
                if (fieldValue.is_object() &&
                    !fieldValue.contains("baseType") &&
                    !fieldValue.contains("value")) {

                    // Recurse into nested structure
                    FillBufferFromFields(buffer, fieldValue, fieldPath);
                }
                else {
                    // Base type field
                    FieldSpec spec = ParseFieldSpec(fieldValue, fieldName);

                    if (spec.isArray) {
                        // Fill array elements
                        for (uint32_t i = 0; i < spec.arraySize; ++i) {
                            std::string elemPath = fieldPath + "[" + std::to_string(i) + "]";
                            SetFieldValueFromSpec(buffer, elemPath, spec.value[i], spec.baseType);
                        }
                    }
                    else {
                        // Fill single value
                        SetFieldValueFromSpec(buffer, fieldPath, spec.value, spec.baseType);
                    }
                }
            }
        }

        // Reconstruct buffer from JSON with explicit types
        std::shared_ptr<ShaderLib::BufferObjectInstance> ReconstructBufferFromJson(
            const json& bufferJson,
            const std::string& bufferName,
            ShaderLib::BufferType bufferType,
            ShaderLib::LayoutStandard layoutStandard)
        {
            // Step 1: Create structure definition
            auto structDef = ShaderLib::MakeStruct(bufferName);

            // Step 2: Build structure from fields recursively
            BuildStructureFromFields(structDef, bufferJson);

            // Step 3: Create BufferLayout from structure
            auto layout = std::make_shared<ShaderLib::BufferLayout>(
                structDef,
                layoutStandard
            );

            // Step 4: Create BufferObjectDefinition
            auto bufferDef = std::make_shared<ShaderLib::BufferObjectDefinition>(
                layout,
                bufferType
            );

            // Step 5: Create BufferObjectInstance
            auto bufferInstance = bufferDef->CreateInstance();

            // Step 6: Fill with values recursively
            FillBufferFromFields(bufferInstance, bufferJson);

            return bufferInstance;
        }

        // Deserialize BufferObjectInstance from full JSON format (with definition)
        std::shared_ptr<ShaderLib::BufferObjectInstance> DeserializeBufferInstance(
            const json& j)
        {
            if (j.is_null()) {
                return nullptr;
            }

            // BufferObjectInstance::ToJson() produces format with 'definition' and 'fields'
            if (!j.contains("definition")) {
                throw std::runtime_error(
                    "Invalid buffer instance JSON: missing 'definition' field"
                );
            }

            // Reconstruct definition first
            auto bufferDef = ShaderLib::BufferObjectDefinition::FromJson(j.at("definition"));

            // Create instance
            auto instance = bufferDef->CreateInstance();

            // Load field values
            if (!instance->FromJson(j)) {
                throw std::runtime_error("Failed to deserialize buffer instance fields");
            }

            return instance;
        }
    }

    // ============================================================================
    // BINARY SERIALIZATION
    // ============================================================================

    AssetData WriteMaterial(
        const std::string& source,
        const MaterialDefinition& material,
        CompressionType compression,
        int compressionLevel)
    {
        if (!material.Validate()) {
            throw std::runtime_error("Invalid material");
        }

        std::vector<uint8_t> binaryData;

        MaterialHeader header{};
        CopyToArray(header.shaderName, material.shaderName);
        header.samplerCount = static_cast<uint32_t>(material.samplers.size());

        std::string inputBufferJson, outputBufferJson, inputOutputBufferJson;

        // IMPORTANT: Serialize with FULL definition (not just fields)
        if (material.inputBuffer) {
            inputBufferJson = material.inputBuffer->ToJson().dump();
            header.inputBufferSize = static_cast<uint32_t>(inputBufferJson.size());
        }

        if (material.outputBuffer) {
            outputBufferJson = material.outputBuffer->ToJson().dump();
            header.outputBufferSize = static_cast<uint32_t>(outputBufferJson.size());
        }

        if (material.inputOutputBuffer) {
            inputOutputBufferJson = material.inputOutputBuffer->ToJson().dump();
            header.inputOutputBufferSize = static_cast<uint32_t>(inputOutputBufferJson.size());
        }

        header.totalDataSize = header.inputBufferSize + header.outputBufferSize +
            header.inputOutputBufferSize;

        // Write header
        const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
        binaryData.insert(binaryData.end(), headerBytes, headerBytes + sizeof(MaterialHeader));

        // Write buffer JSONs
        if (!inputBufferJson.empty()) {
            binaryData.insert(binaryData.end(), inputBufferJson.begin(), inputBufferJson.end());
        }
        if (!outputBufferJson.empty()) {
            binaryData.insert(binaryData.end(), outputBufferJson.begin(), outputBufferJson.end());
        }
        if (!inputOutputBufferJson.empty()) {
            binaryData.insert(binaryData.end(), inputOutputBufferJson.begin(),
                inputOutputBufferJson.end());
        }

        // Write samplers
        for (const auto& sampler : material.samplers) {
            BinarySamplerConfig binSampler{};
            CopyToArray(binSampler.name, sampler.name);
            binSampler.descriptorType = sampler.descriptorType;
            binSampler.binding = sampler.binding;
            CopyToArray(binSampler.texturePath, sampler.texturePath);
            binSampler.colorSpace = sampler.colorSpace;
            binSampler.magFilter = sampler.magFilter;
            binSampler.minFilter = sampler.minFilter;
            binSampler.addressModeU = sampler.addressModeU;
            binSampler.addressModeV = sampler.addressModeV;
            binSampler.addressModeW = sampler.addressModeW;
            binSampler.anisotropy = sampler.anisotropy;
            binSampler.minLod = sampler.minLod;
            binSampler.maxLod = sampler.maxLod;

            const uint8_t* samplerBytes = reinterpret_cast<const uint8_t*>(&binSampler);
            binaryData.insert(binaryData.end(), samplerBytes,
                samplerBytes + sizeof(BinarySamplerConfig));
        }

        // Create asset
        AssetData asset;
        asset.header.assetType = AssetType::Material;
        asset.header.compression = compression;
        asset.header.decompressedSize = static_cast<uint32_t>(binaryData.size());

        asset.metadata["source"] = source;
        asset.metadata["shader"] = material.shaderName;
        asset.metadata["textures"] = material.GetTextureDependencies();

        json textureColorSpaces = json::object();
        for (const auto& sampler : material.samplers) {
            textureColorSpaces[sampler.texturePath] = ColorSpaceToString(sampler.colorSpace);
        }
        asset.metadata["textureColorSpaces"] = textureColorSpaces;

        asset.compressedData = Compress(
            binaryData.data(),
            binaryData.size(),
            compression,
            compressionLevel
        );

        return asset;
    }

    MaterialDefinition ReadMaterial(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }

        std::vector<uint8_t> binaryData = Decompress(
            asset.compressedData.data(),
            asset.compressedData.size(),
            asset.header.decompressedSize
        );

        if (binaryData.size() < sizeof(MaterialHeader)) {
            throw std::runtime_error("Invalid material data: too small");
        }

        const MaterialHeader* header = reinterpret_cast<const MaterialHeader*>(binaryData.data());
        size_t offset = sizeof(MaterialHeader);

        MaterialDefinition material;
        material.shaderName = ExtractFromArray(header->shaderName);

        // FIXED: Deserialize buffers from stored JSON (full format with definition)
        if (header->inputBufferSize > 0) {
            std::string bufferJson(
                reinterpret_cast<const char*>(binaryData.data() + offset),
                header->inputBufferSize
            );
            offset += header->inputBufferSize;

            json j = json::parse(bufferJson);
            material.inputBuffer = DeserializeBufferInstance(j);
        }

        if (header->outputBufferSize > 0) {
            std::string bufferJson(
                reinterpret_cast<const char*>(binaryData.data() + offset),
                header->outputBufferSize
            );
            offset += header->outputBufferSize;

            json j = json::parse(bufferJson);
            material.outputBuffer = DeserializeBufferInstance(j);
        }

        if (header->inputOutputBufferSize > 0) {
            std::string bufferJson(
                reinterpret_cast<const char*>(binaryData.data() + offset),
                header->inputOutputBufferSize
            );
            offset += header->inputOutputBufferSize;

            json j = json::parse(bufferJson);
            material.inputOutputBuffer = DeserializeBufferInstance(j);
        }

        // Deserialize samplers
        for (uint32_t i = 0; i < header->samplerCount; ++i) {
            if (offset + sizeof(BinarySamplerConfig) > binaryData.size()) {
                throw std::runtime_error("Invalid material data: truncated");
            }

            const BinarySamplerConfig* binSampler =
                reinterpret_cast<const BinarySamplerConfig*>(binaryData.data() + offset);
            offset += sizeof(BinarySamplerConfig);

            SamplerDescription sampler;
            sampler.name = ExtractFromArray(binSampler->name);
            sampler.descriptorType = binSampler->descriptorType;
            sampler.binding = binSampler->binding;
            sampler.texturePath = ExtractFromArray(binSampler->texturePath);
            sampler.colorSpace = binSampler->colorSpace;
            sampler.magFilter = binSampler->magFilter;
            sampler.minFilter = binSampler->minFilter;
            sampler.addressModeU = binSampler->addressModeU;
            sampler.addressModeV = binSampler->addressModeV;
            sampler.addressModeW = binSampler->addressModeW;
            sampler.anisotropy = binSampler->anisotropy;
            sampler.minLod = binSampler->minLod;
            sampler.maxLod = binSampler->maxLod;

            material.samplers.push_back(sampler);
        }

        if (!material.Validate()) {
            throw std::runtime_error("Deserialized invalid material");
        }

        return material;
    }

    std::string GetMaterialShaderName(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }
        return asset.metadata.at("shader").get<std::string>();
    }

    std::vector<std::string> GetMaterialTextureDependencies(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }
        return asset.metadata.at("textures").get<std::vector<std::string>>();
    }

    std::unordered_map<std::string, ColorSpace> GetMaterialTextureColorSpaces(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }

        std::unordered_map<std::string, ColorSpace> result;

        if (asset.metadata.contains("textureColorSpaces")) {
            const auto& colorSpacesJson = asset.metadata.at("textureColorSpaces");
            for (const auto& [texturePath, colorSpaceStr] : colorSpacesJson.items()) {
                result[texturePath] = StringToColorSpace(colorSpaceStr.get<std::string>());
            }
        }

        return result;
    }

    // ============================================================================
    // JSON SERIALIZATION 
    // ============================================================================

    SamplerDescription SamplerConfigFromJson(const json& j) {
        SamplerDescription config;
        config.name = j.at("name").get<std::string>();
        config.descriptorType = ShaderLib::StringToDescriptorType(j.at("descriptorType").get<std::string>());
        config.binding = j.at("binding").get<uint32_t>();
        config.texturePath = j.at("texturePath").get<std::string>();
        config.colorSpace = StringToColorSpace(j.at("colorSpace").get<std::string>());
        config.magFilter = StringToFilter(j.at("magFilter").get<std::string>());
        config.minFilter = StringToFilter(j.at("minFilter").get<std::string>());
        config.addressModeU = StringToAddressMode(j.at("addressModeU").get<std::string>());
        config.addressModeV = StringToAddressMode(j.at("addressModeV").get<std::string>());
        config.addressModeW = StringToAddressMode(j.at("addressModeW").get<std::string>());
        config.anisotropy = j.at("anisotropy").get<float>();
        config.minLod = j.at("minLod").get<float>();
        config.maxLod = j.at("maxLod").get<float>();
        return config;
    }

    MaterialDefinition MaterialFromJson(const json& j) {
        MaterialDefinition material;
        material.shaderName = j.at("shader").get<std::string>();

        // Reconstruct buffers with explicit types
        if (j.contains("inputBuffer")) {
            material.inputBuffer = ReconstructBufferFromJson(
                j["inputBuffer"],
                "InputData",
                ShaderLib::BufferType::Uniform,
                ShaderLib::LayoutStandard::Std140
            );
        }

        if (j.contains("outputBuffer")) {
            material.outputBuffer = ReconstructBufferFromJson(
                j["outputBuffer"],
                "OutputData",
                ShaderLib::BufferType::Storage,
                ShaderLib::LayoutStandard::Std430
            );
        }

        if (j.contains("inputOutputBuffer")) {
            material.inputOutputBuffer = ReconstructBufferFromJson(
                j["inputOutputBuffer"],
                "InputOutputData",
                ShaderLib::BufferType::Storage,
                ShaderLib::LayoutStandard::Std430
            );
        }

        if (j.contains("samplers") && j["samplers"].is_array()) {
            for (const auto& samplerJson : j["samplers"]) {
                material.samplers.push_back(SamplerConfigFromJson(samplerJson));
            }
        }

        material.NormalizeSamplerBindings();

        if (!material.Validate()) {
            throw std::runtime_error("Invalid material structure");
        }

        return material;
    }

    // ============================================================================
    // STRING CONVERSIONS
    // ============================================================================

    std::string ColorSpaceToString(ColorSpace cs) {
        switch (cs) {
        case ColorSpace::Linear: return "Linear";
        case ColorSpace::SRGB: return "sRGB";
        case ColorSpace::HDR: return "HDR";
        default: return "Linear";
        }
    }

    ColorSpace StringToColorSpace(const std::string& str) {
        if (str == "sRGB" || str == "SRGB") return ColorSpace::SRGB;
        if (str == "HDR") return ColorSpace::HDR;
        return ColorSpace::Linear;
    }

    std::string FilterToString(SamplerDescription::Filter filter) {
        return filter == SamplerDescription::Filter::Linear ? "Linear" : "Nearest";
    }

    SamplerDescription::Filter StringToFilter(const std::string& str) {
        return str == "Linear" ? SamplerDescription::Filter::Linear : SamplerDescription::Filter::Nearest;
    }

    std::string AddressModeToString(SamplerDescription::AddressMode mode) {
        switch (mode) {
        case SamplerDescription::AddressMode::Repeat: return "Repeat";
        case SamplerDescription::AddressMode::MirroredRepeat: return "MirroredRepeat";
        case SamplerDescription::AddressMode::ClampToEdge: return "ClampToEdge";
        case SamplerDescription::AddressMode::ClampToBorder: return "ClampToBorder";
        default: return "Repeat";
        }
    }

    SamplerDescription::AddressMode StringToAddressMode(const std::string& str) {
        if (str == "MirroredRepeat") return SamplerDescription::AddressMode::MirroredRepeat;
        if (str == "ClampToEdge") return SamplerDescription::AddressMode::ClampToEdge;
        if (str == "ClampToBorder") return SamplerDescription::AddressMode::ClampToBorder;
        return SamplerDescription::AddressMode::Repeat;
    }

} // namespace AssetLib
