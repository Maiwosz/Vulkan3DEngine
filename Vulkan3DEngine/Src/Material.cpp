#include "Material.h"
#include "ShaderModuleManager.h"
#include <stdexcept>

Material::Material(
    const std::string& name,
    ShaderHandle shader,
    const std::vector<Parameter>& params,
    UniformBufferHandle uniformBuffer
) : m_name(name), m_shader(shader), m_uniformBuffer(uniformBuffer), m_parameters(params) {
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

void Material::updateUniformBuffer(ShaderModuleManager& shaderManager) {
    // Use a temporary buffer to pack all the uniform data
    std::vector<uint8_t> uniformData;

    for (const auto& param : m_parameters) {
        // Skip texture parameters as they're not part of the uniform buffer
        if (std::holds_alternative<TextureParam>(param.value)) {
            continue;
        }

        if (param.descriptorType == AssetLib::DescriptorType::UniformBuffer) {
            // Handle different uniform types
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, FloatParam>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, value.value);
                }
                else if constexpr (std::is_same_v<T, Vec2Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<float, 2>{value.x, value.y});
                }
                else if constexpr (std::is_same_v<T, Vec3Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<float, 3>{value.x, value.y, value.z});
                }
                else if constexpr (std::is_same_v<T, Vec4Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<float, 4>{value.x, value.y, value.z, value.w});
                }
                else if constexpr (std::is_same_v<T, IntParam>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, value.value);
                }
                else if constexpr (std::is_same_v<T, IVec2Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<int32_t, 2>{value.x, value.y});
                }
                else if constexpr (std::is_same_v<T, IVec3Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<int32_t, 3>{value.x, value.y, value.z});
                }
                else if constexpr (std::is_same_v<T, IVec4Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<int32_t, 4>{value.x, value.y, value.z, value.w});
                }
                else if constexpr (std::is_same_v<T, UintParam>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, value.value);
                }
                else if constexpr (std::is_same_v<T, UVec2Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<uint32_t, 2>{value.x, value.y});
                }
                else if constexpr (std::is_same_v<T, UVec3Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<uint32_t, 3>{value.x, value.y, value.z});
                }
                else if constexpr (std::is_same_v<T, UVec4Param>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name,
                        std::array<uint32_t, 4>{value.x, value.y, value.z, value.w});
                }
                else if constexpr (std::is_same_v<T, BoolParam>) {
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, value.value ? 1u : 0u);
                }
                else if constexpr (std::is_same_v<T, Mat2Param>) {
                    // Flatten the matrix for updating
                    std::array<float, 4> flatMatrix;
                    for (int i = 0; i < 2; ++i) {
                        for (int j = 0; j < 2; ++j) {
                            flatMatrix[i * 2 + j] = value.data[i][j];
                        }
                    }
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, flatMatrix);
                }
                else if constexpr (std::is_same_v<T, Mat3Param>) {
                    // Flatten the matrix for updating
                    std::array<float, 9> flatMatrix;
                    for (int i = 0; i < 3; ++i) {
                        for (int j = 0; j < 3; ++j) {
                            flatMatrix[i * 3 + j] = value.data[i][j];
                        }
                    }
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, flatMatrix);
                }
                else if constexpr (std::is_same_v<T, Mat4Param>) {
                    // Flatten the matrix for updating
                    std::array<float, 16> flatMatrix;
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            flatMatrix[i * 4 + j] = value.data[i][j];
                        }
                    }
                    shaderManager.updateUniformVariable(m_uniformBuffer, param.name, flatMatrix);
                }
                }, param.value);
        }
    }
}