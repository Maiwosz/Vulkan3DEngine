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

// Forward declarations
class MaterialManager;

class Material {
public:
    // Parameter variants that a material can have
    struct FloatParam { float value; };
    struct Vec2Param { float x, y; };
    struct Vec3Param { float x, y, z; };
    struct Vec4Param { float x, y, z, w; };
    struct IntParam { int32_t value; };
    struct IVec2Param { int32_t x, y; };
    struct IVec3Param { int32_t x, y, z; };
    struct IVec4Param { int32_t x, y, z, w; };
    struct UintParam { uint32_t value; };
    struct UVec2Param { uint32_t x, y; };
    struct UVec3Param { uint32_t x, y, z; };
    struct UVec4Param { uint32_t x, y, z, w; };
    struct BoolParam { bool value; };
    struct Mat2Param { float data[2][2]; };
    struct Mat3Param { float data[3][3]; };
    struct Mat4Param { float data[4][4]; };
    struct TextureParam {
        AssetHandle handle;
        VramHandle vramHandle;  // Will be populated during ensureReady
        VkSampler sampler;      // Sampler for this texture
    };

    // Parameter value variant
    using ParamValue = std::variant<
        FloatParam, Vec2Param, Vec3Param, Vec4Param,
        IntParam, IVec2Param, IVec3Param, IVec4Param,
        UintParam, UVec2Param, UVec3Param, UVec4Param,
        BoolParam, Mat2Param, Mat3Param, Mat4Param,
        TextureParam
    >;

    // Parameter structure
    struct Parameter {
        std::string name;
        ParamValue value;
        AssetLib::DescriptorType descriptorType;
        uint32_t binding;
        uint32_t arrayIndex;  // For array parameters, 0 for non-array params
    };

    // Constructor
    Material(
        const std::string& name,
        ShaderHandle shader,
        const std::vector<Parameter>& params,
        UniformBufferHandle uniformBuffer
    );

    // Destructor
    ~Material();

    // Accessors
    const std::string& name() const { return m_name; }
    ShaderHandle shader() const { return m_shader; }
    UniformBufferHandle uniformBuffer() const { return m_uniformBuffer; }
    const std::vector<Parameter>& parameters() const { return m_parameters; }
    std::vector<Parameter>& parameters() { return m_parameters; }

    // Parameter management
    bool setParameter(const std::string& name, const ParamValue& value);
    bool getParameter(const std::string& name, ParamValue& outValue) const;

    // Update all parameters to the uniform buffer
    void updateUniformBuffer(ShaderModuleManager& shaderManager);

private:
    std::string m_name;
    ShaderHandle m_shader;
    UniformBufferHandle m_uniformBuffer;
    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, size_t> m_parameterIndices;  // Name to index mapping for quick lookups

    friend class MaterialManager;
};