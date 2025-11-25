#include "pch.h"
#include "MaterialSerializer.h"
#include "Serialization.h"
#include "TypeSerializationTable.h"
#include <algorithm>
#include <cstring>
#include "AssetLib.h"

namespace AssetLib {

    // ============================================================================
    // HELPERS
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

        // Set field value from JSON by path
        void SetFieldValueFromJson(
            std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
            const std::string& fieldPath,
            const json& value,
            ShaderLib::BaseType expectedType)
        {
            const auto& info = ShaderLib::GetSerializationInfo(expectedType);
            (*buffer)[fieldPath] = info.fromJson(value);
        }

        // Recursively fill buffer fields from JSON values
        void FillBufferFieldsFromJson(
            std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
            const json& fieldValues,
            const std::string& parentPath = "")
        {
            if (!fieldValues.is_object()) {
                return;
            }

            const auto& layout = buffer->GetLayout();

            for (const auto& [fieldName, fieldValue] : fieldValues.items()) {
                std::string fieldPath = parentPath.empty()
                    ? fieldName
                    : parentPath + "." + fieldName;

                // Look up field in layout
                const auto* fieldDesc = layout->FindField(fieldPath);

                if (!fieldDesc) {
                    // Field not found in shader definition - skip silently
                    // This is expected: asset may have old/extra fields
                    continue;
                }

                if (!fieldDesc->isBaseType) {
                    // Nested structure - recurse
                    if (fieldValue.is_object()) {
                        FillBufferFieldsFromJson(buffer, fieldValue, fieldPath);
                    }
                }
                else {
                    // Base type field
                    if (fieldDesc->isArray) {
                        // Array field
                        if (!fieldValue.is_array()) {
                            continue; // Skip if value is not an array
                        }

                        uint32_t elemCount = std::min(
                            static_cast<uint32_t>(fieldValue.size()),
                            fieldDesc->arraySize
                        );

                        for (uint32_t i = 0; i < elemCount; ++i) {
                            std::string elemPath = fieldPath + "[" + std::to_string(i) + "]";
                            try {
                                SetFieldValueFromJson(
                                    buffer,
                                    elemPath,
                                    fieldValue[i],
                                    fieldDesc->baseType
                                );
                            }
                            catch (...) {
                                // Skip incompatible values
                            }
                        }
                    }
                    else {
                        // Single value
                        try {
                            SetFieldValueFromJson(
                                buffer,
                                fieldPath,
                                fieldValue,
                                fieldDesc->baseType
                            );
                        }
                        catch (...) {
                            // Skip incompatible values
                        }
                    }
                }
            }
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
        header.bufferCount = static_cast<uint32_t>(material.buffers.size());
        header.samplerCount = static_cast<uint32_t>(material.samplers.size());

        // Prepare buffer entries and JSONs
        std::vector<BinaryBufferEntry> bufferEntries;
        std::vector<std::string> bufferJsons;
        bufferEntries.reserve(material.buffers.size());
        bufferJsons.reserve(material.buffers.size());

        uint32_t totalBufferDataSize = 0;

        for (const auto& [bufferName, bufferValues] : material.buffers) {
            BinaryBufferEntry entry{};
            CopyToArray(entry.bufferName, bufferName);

            std::string bufferJson = bufferValues.dump();
            entry.dataSize = static_cast<uint32_t>(bufferJson.size());

            totalBufferDataSize += entry.dataSize;

            bufferEntries.push_back(entry);
            bufferJsons.push_back(std::move(bufferJson));
        }

        header.totalBufferDataSize = totalBufferDataSize;

        // Write header
        const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
        binaryData.insert(binaryData.end(), headerBytes, headerBytes + sizeof(MaterialHeader));

        // Write buffer entries
        for (const auto& entry : bufferEntries) {
            const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
            binaryData.insert(binaryData.end(), entryBytes, entryBytes + sizeof(BinaryBufferEntry));
        }

        // Write buffer JSONs
        for (const auto& bufferJson : bufferJsons) {
            binaryData.insert(binaryData.end(), bufferJson.begin(), bufferJson.end());
        }

        // Write samplers
        for (const auto& sampler : material.samplers) {
            BinarySamplerConfig binSampler{};
            CopyToArray(binSampler.name, sampler.name);
            binSampler.descriptorType = sampler.descriptorType;
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

        // Read buffer entries
        std::vector<BinaryBufferEntry> bufferEntries;
        bufferEntries.reserve(header->bufferCount);

        for (uint32_t i = 0; i < header->bufferCount; ++i) {
            if (offset + sizeof(BinaryBufferEntry) > binaryData.size()) {
                throw std::runtime_error("Invalid material data: truncated buffer entries");
            }

            const BinaryBufferEntry* entry =
                reinterpret_cast<const BinaryBufferEntry*>(binaryData.data() + offset);
            offset += sizeof(BinaryBufferEntry);

            bufferEntries.push_back(*entry);
        }

        // Read buffer JSONs
        for (const auto& entry : bufferEntries) {
            if (offset + entry.dataSize > binaryData.size()) {
                throw std::runtime_error("Invalid material data: truncated buffer JSON");
            }

            std::string bufferJson(
                reinterpret_cast<const char*>(binaryData.data() + offset),
                entry.dataSize
            );
            offset += entry.dataSize;

            std::string bufferName = ExtractFromArray(entry.bufferName);
            material.buffers[bufferName] = json::parse(bufferJson);
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
    // BUFFER INSTANCE CREATION
    // ============================================================================

    std::shared_ptr<ShaderLib::BufferObjectInstance> CreateBufferInstanceFromMaterial(
        std::shared_ptr<const ShaderLib::BufferObjectDefinition> shaderBufferDef,
        const json& materialFieldValues)
    {
        if (!shaderBufferDef) {
            return nullptr;
        }

        // Create instance from shader definition (with default values)
        auto instance = shaderBufferDef->CreateInstance();

        // Fill with material values (matching fields by name)
        // Fields not in materialFieldValues keep their defaults
        // Extra fields in materialFieldValues are ignored
        if (!materialFieldValues.is_null() && materialFieldValues.is_object()) {
            FillBufferFieldsFromJson(instance, materialFieldValues);
        }

        return instance;
    }

    // ============================================================================
    // JSON SERIALIZATION 
    // ============================================================================

    SamplerDescription SamplerConfigFromJson(const json& j) {
        SamplerDescription config;
        config.name = j.at("name").get<std::string>();
        config.descriptorType = ShaderLib::StringToDescriptorType(j.at("descriptorType").get<std::string>());
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

        // Parse buffers - support two formats:
        // 1. Nested format: "buffers": { "BufferName1": {...}, "BufferName2": {...} }
        // 2. Flat format: "BufferName1": {...}, "BufferName2": {...} at top level

        if (j.contains("buffers") && j["buffers"].is_object()) {
            // Nested format
            for (const auto& [bufferName, bufferValues] : j["buffers"].items()) {
                if (bufferValues.is_object()) {
                    material.buffers[bufferName] = bufferValues;
                }
            }
        }
        else {
            // Flat format - scan for objects that aren't "shader" or "samplers"
            for (const auto& [key, value] : j.items()) {
                if (key == "shader" || key == "samplers") {
                    continue;
                }

                if (value.is_object()) {
                    // This is a buffer
                    material.buffers[key] = value;
                }
            }
        }

        if (j.contains("samplers") && j["samplers"].is_array()) {
            for (const auto& samplerJson : j["samplers"]) {
                material.samplers.push_back(SamplerConfigFromJson(samplerJson));
            }
        }

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
