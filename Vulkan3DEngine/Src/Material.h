#pragma once
#include "AssetHandle.h"
#include "ShaderReflection.h"
#include <variant>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "VramAssetTypes.h"
#include <vulkan/vulkan.h>

class Material {
public:
    struct TextureParam {
        AssetHandle handle;
        AssetLib::SamplerDescription sampler;
        VkSampler samplerHandle = VK_NULL_HANDLE;
    };

    using Value = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        int32_t,
        uint32_t,
        bool,
        TextureParam
    >;

    struct Parameter {
        std::string name;
        Value value;
        uint32_t arraySize = 1;
    };

    Material(CombinedShader shader, std::vector<Parameter> parameters);

    // Accessors
    const CombinedShader& shader() const { return m_shader; }
    const std::vector<Parameter>& parameters() const { return m_parameters; }
    std::vector<Parameter>& parameters() { return m_parameters; }

    // Parameter lookup
    const Parameter* findParameter(const std::string& name) const;
    Parameter* findParameter(const std::string& name);
    std::vector<TextureParam> textureParameters() const;

private:
    CombinedShader m_shader;
    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, size_t> m_parameterLookup;

    void buildLookup();
};