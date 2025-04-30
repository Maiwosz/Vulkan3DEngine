#include "Material.h"

Material::Material(CombinedShader shader, std::vector<Parameter> parameters)
    : m_shader(std::move(shader)),
    m_parameters(std::move(parameters)) {
    buildLookup();
}

void Material::buildLookup() {
    m_parameterLookup.clear();
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameterLookup[m_parameters[i].name] = i;
    }
}

const Material::Parameter* Material::findParameter(const std::string& name) const {
    auto it = m_parameterLookup.find(name);
    return it != m_parameterLookup.end() ? &m_parameters[it->second] : nullptr;
}

Material::Parameter* Material::findParameter(const std::string& name)
{
    auto it = m_parameterLookup.find(name);
    if (it != m_parameterLookup.end()) {
        return &m_parameters[it->second];
    }
    return nullptr;
}

std::vector<Material::TextureParam> Material::textureParameters() const {
    std::vector<TextureParam> textures;
    for (const auto& param : m_parameters) {
        if (const TextureParam* tex = std::get_if<TextureParam>(&param.value)) {
            textures.push_back(*tex);
        }
    }
    return textures;
}