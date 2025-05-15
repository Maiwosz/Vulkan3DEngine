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
#include <glm/glm.hpp>

// Forward declarations
class MaterialManager;

class Material {
public:
    // Texture parameter type (not covered by ShaderLib's UniformTypeTraits)
    struct TextureParam {
        AssetHandle handle;
        VramHandle vramHandle;  // Will be populated during ensureReady
        VkSampler sampler;      // Sampler for this texture
    };

    // Parameter value variant that uses native GLM types
    using ParamValue = std::variant<
        bool,                // for UniformType::Bool
        float,               // for UniformType::Float
        glm::vec2,           // for UniformType::Vec2
        glm::vec3,           // for UniformType::Vec3
        glm::vec4,           // for UniformType::Vec4
        int32_t,             // for UniformType::Int
        glm::ivec2,          // for UniformType::IVec2
        glm::ivec3,          // for UniformType::IVec3
        glm::ivec4,          // for UniformType::IVec4
        uint32_t,            // for UniformType::UInt
        glm::uvec2,          // for UniformType::UVec2
        glm::uvec3,          // for UniformType::UVec3
        glm::uvec4,          // for UniformType::UVec4
        glm::mat2,           // for UniformType::Mat2
        glm::mat3,           // for UniformType::Mat3
        glm::mat4,           // for UniformType::Mat4
        TextureParam         // for texture parameters
    >;

    // Parameter structure
    struct Parameter {
        std::string name;
        ParamValue value;
        ShaderLib::DescriptorType descriptorType;
        ShaderLib::UniformType uniformType;  // Added to track the actual uniform type
        uint32_t binding;
        uint32_t arrayIndex;  // For array parameters, 0 for non-array params
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

private:
    std::string m_name;
    ShaderHandle m_shader;
    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, size_t> m_parameterIndices;  // Name to index mapping for quick lookups

    friend class MaterialManager;
};