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

    // Check if the variant types match
    if (value.index() != m_parameters[it->second].value.index()) {
        return false;  // Type mismatch
    }

    m_parameters[it->second].value = value;
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