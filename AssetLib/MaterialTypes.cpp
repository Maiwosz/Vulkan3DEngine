#include "pch.h"
#include "MaterialTypes.h"
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <algorithm>
#include <stdexcept>

namespace AssetLib {

    // ============================================================================
    // PARAMETER VALUE - IMPLEMENTATION
    // ============================================================================

    bool ParameterValue::IsTexture() const {
        const auto& info = ShaderLib::GetDescriptorTypeInfo(descriptorType);
        return info.IsTexture();
    }

    bool ParameterValue::IsBuffer() const {
        const auto& info = ShaderLib::GetDescriptorTypeInfo(descriptorType);
        return info.IsBuffer();
    }

    bool ParameterValue::IsSampler() const {
        return descriptorType == ShaderLib::DescriptorType::Sampler;
    }

    const ShaderLib::BufferValue& ParameterValue::GetBaseValue() const {
        if (!std::holds_alternative<ShaderLib::BufferValue>(value)) {
            throw std::runtime_error("Parameter '" + name + "' does not hold a BufferValue");
        }
        return std::get<ShaderLib::BufferValue>(value);
    }

    std::shared_ptr<ShaderLib::CompositeTypeInstance> ParameterValue::GetComposite() const {
        if (!IsComposite()) {
            throw std::runtime_error("Parameter '" + name + "' is not a composite type");
        }

        const auto& bufferVal = GetBaseValue();

        // Try to get ShaderStructInstance
        if (auto* structPtr = std::get_if<std::shared_ptr<ShaderLib::ShaderStructInstance>>(&bufferVal)) {
            return *structPtr;
        }

        // Try to get ShaderArrayInstance
        if (auto* arrayPtr = std::get_if<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(&bufferVal)) {
            return *arrayPtr;
        }

        throw std::runtime_error("Parameter '" + name + "' BufferValue does not contain a composite type");
    }

    const std::string& ParameterValue::GetTexturePath() const {
        if (!IsTexturePath()) {
            throw std::runtime_error("Parameter '" + name + "' is not a texture path");
        }
        return std::get<std::string>(value);
    }

    bool ParameterValue::Validate() const {
        // Check name
        if (name.empty()) {
            return false;
        }

        // Check array size
        if (arraySize == 0) {
            return false;
        }

        // Validate based on descriptor type
        const auto& descInfo = ShaderLib::GetDescriptorTypeInfo(descriptorType);

        if (descInfo.IsBuffer()) {
            // Buffers must have BaseType, Struct, or Array
            if (valueType == ParameterValueType::TexturePath) {
                return false;
            }

            if (!std::holds_alternative<ShaderLib::BufferValue>(value)) {
                return false;
            }

            const auto& bufferVal = std::get<ShaderLib::BufferValue>(value);

            if (valueType == ParameterValueType::BaseType) {
                if (baseType == ShaderLib::BaseType::Unknown) {
                    return false;
                }
                // Verify that BufferValue holds a base type (not a composite)
                if (std::holds_alternative<std::shared_ptr<ShaderLib::ShaderStructInstance>>(bufferVal) ||
                    std::holds_alternative<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(bufferVal)) {
                    return false;
                }
            }
            else if (valueType == ParameterValueType::Struct) {
                // Verify that BufferValue holds a struct
                if (!std::holds_alternative<std::shared_ptr<ShaderLib::ShaderStructInstance>>(bufferVal)) {
                    return false;
                }
                auto structPtr = std::get<std::shared_ptr<ShaderLib::ShaderStructInstance>>(bufferVal);
                if (!structPtr) {
                    return false;
                }
            }
            else if (valueType == ParameterValueType::Array) {
                // Verify that BufferValue holds an array
                if (!std::holds_alternative<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(bufferVal)) {
                    return false;
                }
                auto arrayPtr = std::get<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(bufferVal);
                if (!arrayPtr) {
                    return false;
                }
            }
        }
        else if (descInfo.IsTexture() || descInfo.IsImage()) {
            // Textures must have TexturePath
            if (valueType != ParameterValueType::TexturePath) {
                return false;
            }
            if (!std::holds_alternative<std::string>(value)) {
                return false;
            }
            // Check if path is not empty
            if (std::get<std::string>(value).empty()) {
                return false;
            }
        }
        else if (descriptorType == ShaderLib::DescriptorType::Sampler) {
            // Samplers should have BaseType with Unknown base type
            if (valueType != ParameterValueType::BaseType) {
                return false;
            }
            // Sampler has no data, just descriptor
        }

        return true;
    }

    // ============================================================================
    // MATERIAL - IMPLEMENTATION
    // ============================================================================

    const ParameterValue* MaterialDefinition::FindParameter(const std::string& name) const {
        auto it = std::find_if(parameters.begin(), parameters.end(),
            [&name](const ParameterValue& p) { return p.name == name; });
        return (it != parameters.end()) ? &(*it) : nullptr;
    }

    ParameterValue* MaterialDefinition::FindParameter(const std::string& name) {
        auto it = std::find_if(parameters.begin(), parameters.end(),
            [&name](const ParameterValue& p) { return p.name == name; });
        return (it != parameters.end()) ? &(*it) : nullptr;
    }

    bool MaterialDefinition::HasParameter(const std::string& name) const {
        return FindParameter(name) != nullptr;
    }

    bool MaterialDefinition::Validate() const {
        if (shaderName.empty()) {
            return false;
        }

        // Validate all parameters
        for (const auto& param : parameters) {
            if (!param.Validate()) {
                return false;
            }
        }

        // Check for duplicate names
        for (size_t i = 0; i < parameters.size(); ++i) {
            for (size_t j = i + 1; j < parameters.size(); ++j) {
                if (parameters[i].name == parameters[j].name) {
                    return false;
                }
            }
        }

        return true;
    }

    std::vector<const ParameterValue*> MaterialDefinition::GetTextureParameters() const {
        std::vector<const ParameterValue*> result;
        for (const auto& param : parameters) {
            if (param.IsTexture()) {
                result.push_back(&param);
            }
        }
        return result;
    }

    std::vector<const ParameterValue*> MaterialDefinition::GetBufferParameters() const {
        std::vector<const ParameterValue*> result;
        for (const auto& param : parameters) {
            if (param.IsBuffer()) {
                result.push_back(&param);
            }
        }
        return result;
    }

    std::vector<const ParameterValue*> MaterialDefinition::GetSamplerParameters() const {
        std::vector<const ParameterValue*> result;
        for (const auto& param : parameters) {
            if (param.IsSampler()) {
                result.push_back(&param);
            }
        }
        return result;
    }

    MaterialDependencies MaterialDefinition::ExtractDependencies() const {
        MaterialDependencies deps;
        deps.shaderName = shaderName;

        for (const auto& param : parameters) {
            if (param.IsTexture() && param.IsTexturePath()) {
                const std::string& texturePath = param.GetTexturePath();
                if (!texturePath.empty()) {
                    deps.textureNames.push_back(texturePath);
                    deps.textureColorSpaces.push_back(param.samplerDesc.colorSpace);
                }
            }
        }

        return deps;
    }

    // ============================================================================
    // STRING CONVERSIONS
    // ============================================================================

    std::string SamplerFilterToString(SamplerDescription::Filter filter) {
        switch (filter) {
        case SamplerDescription::Filter::Nearest: return "Nearest";
        case SamplerDescription::Filter::Linear: return "Linear";
        default: return "Unknown";
        }
    }

    std::string AddressModeToString(SamplerDescription::AddressMode mode) {
        switch (mode) {
        case SamplerDescription::AddressMode::Repeat: return "Repeat";
        case SamplerDescription::AddressMode::MirroredRepeat: return "MirroredRepeat";
        case SamplerDescription::AddressMode::ClampToEdge: return "ClampToEdge";
        case SamplerDescription::AddressMode::ClampToBorder: return "ClampToBorder";
        default: return "Unknown";
        }
    }

    std::string ColorSpaceToString(ColorSpace colorSpace) {
        switch (colorSpace) {
        case ColorSpace::Linear: return "Linear";
        case ColorSpace::SRGB: return "sRGB";
        case ColorSpace::HDR: return "HDR";
        default: return "Unknown";
        }
    }

    std::string ParameterValueTypeToString(ParameterValueType type) {
        switch (type) {
        case ParameterValueType::BaseType: return "BaseType";
        case ParameterValueType::Struct: return "Struct";
        case ParameterValueType::Array: return "Array";
        case ParameterValueType::TexturePath: return "TexturePath";
        default: return "Unknown";
        }
    }

    SamplerDescription::Filter StringToSamplerFilter(const std::string& filter) {
        if (filter == "Nearest") return SamplerDescription::Filter::Nearest;
        if (filter == "Linear") return SamplerDescription::Filter::Linear;
        throw std::runtime_error("Invalid sampler filter: " + filter);
    }

    SamplerDescription::AddressMode StringToAddressMode(const std::string& mode) {
        if (mode == "Repeat") return SamplerDescription::AddressMode::Repeat;
        if (mode == "MirroredRepeat") return SamplerDescription::AddressMode::MirroredRepeat;
        if (mode == "ClampToEdge") return SamplerDescription::AddressMode::ClampToEdge;
        if (mode == "ClampToBorder") return SamplerDescription::AddressMode::ClampToBorder;
        throw std::runtime_error("Invalid address mode: " + mode);
    }

    ColorSpace StringToColorSpace(const std::string& colorSpace) {
        if (colorSpace == "SRGB" || colorSpace == "sRGB") return ColorSpace::SRGB;
        if (colorSpace == "HDR") return ColorSpace::HDR;
        return ColorSpace::Linear;
    }

    ParameterValueType StringToParameterValueType(const std::string& str) {
        if (str == "BaseType") return ParameterValueType::BaseType;
        if (str == "Struct") return ParameterValueType::Struct;
        if (str == "Array") return ParameterValueType::Array;
        if (str == "TexturePath") return ParameterValueType::TexturePath;
        throw std::runtime_error("Invalid ParameterValueType: " + str);
    }

    SamplerDescription GetDefaultSampler() {
        return SamplerDescription{
            SamplerDescription::Filter::Linear,
            SamplerDescription::Filter::Linear,
            SamplerDescription::AddressMode::Repeat,
            SamplerDescription::AddressMode::Repeat,
            SamplerDescription::AddressMode::Repeat,
            1.0f, 0.0f, 1000.0f,
            ColorSpace::Linear
        };
    }

} // namespace AssetLib