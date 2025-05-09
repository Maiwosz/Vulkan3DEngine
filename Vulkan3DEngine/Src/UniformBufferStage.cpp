#include "UniformBufferStage.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <glm/gtc/matrix_transform.hpp>
#include "UBOStandardDefinitions.h"

UniformBufferStage::UniformBufferStage(Registry& registry, Renderer& renderer)
    : m_registry(registry)
    , m_shaderManager(renderer.shaderModuleManager())
    , m_uniformBufferManager(renderer.uniformBufferManager())
    , m_materialManager(renderer.materialManager())
{
    SPDLOG_INFO("Initializing UniformBufferStage");
}

void UniformBufferStage::process(std::shared_ptr<RenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    SPDLOG_DEBUG("Processing render order of type: {}", renderOrderTypeToString(order->getType()));

    switch (order->getType()) {
    case RenderOrderType::Mesh:
        processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
        break;
    default:
        SPDLOG_WARN("Unknown render order type");
        break;
    }

    // Forward the order to the next stage
    forwardToNextStage(order);
}

void UniformBufferStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Null mesh render order provided");
        return;
    }

    // Get material if specified in the order
    Material* material = nullptr;
    if (order->materialHandle) {
        material = m_materialManager.get(order->materialHandle);
        if (!material) {
            SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
        }
    }

    // Get shader handle - either from material or directly from order
    ShaderHandle shaderHandle = material->shader();

    if (!m_shaderManager.isShaderValid(shaderHandle)) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order");
        return;
    }

    // Create and update object UBO
    UniformBufferHandle objectUboHandle = m_shaderManager.createObjectUniformBuffer(shaderHandle);
    if (objectUboHandle) {
        ShaderLib::ObjectUBOData objectUboData;

        auto& transformComponent = m_registry.getComponent<TransformComponent>(order->entity);

        // Fill object UBO data
        objectUboData.model = transformComponent.getModelMatrix();

        // Update buffer with data
        m_uniformBufferManager.updateBuffer(objectUboHandle, &objectUboData, sizeof(objectUboData));

        // Store the handle in the order for later use
        order->objectUBOHandle = objectUboHandle;
    }
    else {
        SPDLOG_WARN("Failed to create object uniform buffer");
    }

    // Process custom UBO (material data) if material is present
    if (material) {
        UniformBufferHandle customUboHandle = m_shaderManager.createCustomUniformBuffer(shaderHandle, "InputData");
        if (customUboHandle) {
            const auto& customUboInfo = m_uniformBufferManager.getBufferInfo(customUboHandle);
            const auto& materialParams = material->parameters();

            // Prepare buffer for custom UBO
            std::vector<uint8_t> customUboData(customUboInfo.size, 0);

            // Map material parameters to custom UBO fields
            for (const auto& param : materialParams) {
                // Find corresponding variable in custom UBO
                bool foundVariable = false;
                for (const auto& variable : customUboInfo.variables) {
                    if (variable.name == param.name) {
                        foundVariable = true;

                        // Copy parameter data to buffer based on type
                        if (std::holds_alternative<Material::FloatParam>(param.value)) {
                            const auto& floatParam = std::get<Material::FloatParam>(param.value);
                            memcpy(customUboData.data() + variable.offset, &floatParam.value, sizeof(float));
                        }
                        else if (std::holds_alternative<Material::Vec2Param>(param.value)) {
                            const auto& vec2Param = std::get<Material::Vec2Param>(param.value);
                            float values[2] = { vec2Param.x, vec2Param.y };
                            memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 2);
                        }
                        else if (std::holds_alternative<Material::Vec3Param>(param.value)) {
                            const auto& vec3Param = std::get<Material::Vec3Param>(param.value);
                            float values[3] = { vec3Param.x, vec3Param.y, vec3Param.z };
                            memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 3);
                        }
                        else if (std::holds_alternative<Material::Vec4Param>(param.value)) {
                            const auto& vec4Param = std::get<Material::Vec4Param>(param.value);
                            float values[4] = { vec4Param.x, vec4Param.y, vec4Param.z, vec4Param.w };
                            memcpy(customUboData.data() + variable.offset, values, sizeof(float) * 4);
                        }
                        else if (std::holds_alternative<Material::IntParam>(param.value)) {
                            const auto& intParam = std::get<Material::IntParam>(param.value);
                            memcpy(customUboData.data() + variable.offset, &intParam.value, sizeof(int32_t));
                        }
                        else if (std::holds_alternative<Material::BoolParam>(param.value)) {
                            const auto& boolParam = std::get<Material::BoolParam>(param.value);
                            int32_t value = boolParam.value ? 1 : 0;  // Booleans are often represented as ints in shaders
                            memcpy(customUboData.data() + variable.offset, &value, sizeof(int32_t));
                        }
                        else if (std::holds_alternative<Material::Mat4Param>(param.value)) {
                            const auto& mat4Param = std::get<Material::Mat4Param>(param.value);
                            memcpy(customUboData.data() + variable.offset, mat4Param.data, sizeof(float) * 16);
                        }
                        break;
                    }
                }

                if (!foundVariable) {
                    SPDLOG_WARN("Material parameter '{}' not found in custom UBO", param.name);
                }
            }

            // Update custom UBO with mapped data
            m_uniformBufferManager.updateBuffer(customUboHandle, customUboData.data(), customUboData.size());

            // Store the handle in the order for later use
            order->materialUBOHandle = customUboHandle;
            SPDLOG_DEBUG("Created and updated custom UBO: handle={}", customUboHandle.id);
        }
        else {
            SPDLOG_WARN("Failed to create custom uniform buffer");
        }
    }
}

