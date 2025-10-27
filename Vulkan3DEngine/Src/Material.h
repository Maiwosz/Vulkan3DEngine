#pragma once
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include "ShaderModuleManager.h"
#include "VramManager.h"
#include "AssetHandle.h"
#include "AssetLib.h"
#include "ImageSamplerManager.h"
#include "ShaderLib.h"
#include "TextureManager.h"
#include <glm/glm.hpp>

// Forward declarations
class MaterialManager;

class Material {
public:
    // Texture parameter type (not covered by ShaderLib's BufferValue)
    struct TextureParam {
        AssetHandle handle;
        TextureHandle textureHandle;
        SamplerHandle samplerHandle;
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB;
    };

    // Parameter value variant - uses ShaderLib::BufferValue for buffer types
    // and extends it with texture support
    using ParamValue = std::variant<
        ShaderLib::BufferValue,  // All buffer-compatible types (UBO/SSBO)
        TextureParam             // Textures (descriptors only)
    >;

    // Parameter structure
    struct Parameter {
        std::string name;
        ParamValue value;
        ShaderLib::DescriptorType descriptorType;
        uint32_t binding;
        uint32_t set;  // Typically CUSTOM_DESCRIPTOR_SET
        uint32_t arrayIndex;  // For array parameters, 0 for non-array params

        // Get the base type if this is a buffer parameter
        ShaderLib::BaseType getBaseType() const {
            if (auto* bufVal = std::get_if<ShaderLib::BufferValue>(&value)) {
                return ShaderLib::GetBaseTypeFromVariant(*bufVal);
            }
            return ShaderLib::BaseType::Unknown;
        }

        bool isBufferParameter() const {
            return descriptorType == ShaderLib::DescriptorType::UniformBuffer ||
                descriptorType == ShaderLib::DescriptorType::StorageBuffer;
        }

        bool isTextureParameter() const {
            return std::holds_alternative<TextureParam>(value);
        }
    };

    // Constructor
    Material(
        const std::string& name,
        ShaderHandle shader,
        const std::vector<Parameter>& params
    );

    // Destructor
    ~Material();

    // Accessors
    const std::string& name() const { return m_name; }
    ShaderHandle shader() const { return m_shader; }
    const std::vector<Parameter>& parameters() const { return m_parameters; }
    std::vector<Parameter>& parameters() { return m_parameters; }

    // Parameter management
    bool setParameter(const std::string& name, const ParamValue& value);
    bool getParameter(const std::string& name, ParamValue& outValue) const;

    // Convenience methods for common types
    template<typename T>
    bool setBufferParameter(const std::string& name, const T& value) {
        static_assert(ShaderLib::BaseTypeTraits<T>::supported,
            "Type not supported by ShaderLib");

        ShaderLib::BufferValue bufVal = value;
        return setParameter(name, ParamValue{ bufVal });
    }

    bool setTextureParameter(const std::string& name, const TextureParam& texture) {
        return setParameter(name, ParamValue{ texture });
    }

private:
    std::string m_name;
    ShaderHandle m_shader;
    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, size_t> m_parameterIndices;  // Name to index mapping

    friend class MaterialManager;
};