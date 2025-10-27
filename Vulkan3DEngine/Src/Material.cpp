#include "Material.h"
#include "ShaderModuleManager.h"
#include <stdexcept>

Material::Material(
    const std::string& name,
    ShaderHandle shader,
    const std::vector<Parameter>& params
)
    : m_name(name),
    m_shader(shader),
    m_parameters(params)
{
    // Build parameter indices map for quick lookup
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameterIndices[m_parameters[i].name] = i;
    }
}

Material::~Material() {
    // Cleanup resources if needed
}

bool Material::setParameter(const std::string& name, const ParamValue& value) {
    auto it = m_parameterIndices.find(name);
    if (it == m_parameterIndices.end()) {
        return false;  // Parameter not found
    }

    Parameter& param = m_parameters[it->second];

    // Type checking based on parameter type
    if (param.isBufferParameter()) {
        // For buffer parameters, value must be BufferValue
        const auto* newBufferVal = std::get_if<ShaderLib::BufferValue>(&value);
        const auto* oldBufferVal = std::get_if<ShaderLib::BufferValue>(&param.value);

        if (!newBufferVal || !oldBufferVal) {
            return false;  // Type mismatch
        }

        // Check if BufferValue variant types match
        if (newBufferVal->index() != oldBufferVal->index()) {
            return false;  // Type mismatch
        }
    }
    else if (param.isTextureParameter()) {
        // For texture parameters, value must be TextureParam
        if (!std::holds_alternative<TextureParam>(value)) {
            return false;  // Type mismatch
        }
    }
    else {
        return false;  // Unknown parameter type
    }

    param.value = value;
    return true;
}

bool Material::getParameter(const std::string& name, ParamValue& outValue) const {
    auto it = m_parameterIndices.find(name);
    if (it == m_parameterIndices.end()) {
        return false;  // Parameter not found
    }

    outValue = m_parameters[it->second].value;
    return true;
}