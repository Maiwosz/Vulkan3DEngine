#pragma once
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <glm/glm.hpp>
#include <memory>

namespace ShaderLib {

    // ============================================================================
    // LIGHT STRUCTURE DEFINITION FACTORIES
    // ============================================================================

    inline std::shared_ptr<ShaderStructDefinition> CreateDirectionalLightType(
        LayoutStandard standard = LayoutStandard::Std140) {

        auto structDef = std::make_shared<ShaderStructDefinition>("DirectionalLight", standard);
        structDef->AddField("direction", BaseType::Vec3);
        structDef->AddField("color", BaseType::Vec4); // w is intensity
        structDef->Finalize();
        return structDef;
    }

    inline std::shared_ptr<ShaderStructDefinition> CreatePointLightType(
        LayoutStandard standard = LayoutStandard::Std140) {

        auto structDef = std::make_shared<ShaderStructDefinition>("PointLight", standard);
        structDef->AddField("position", BaseType::Vec3);
        structDef->AddField("radius", BaseType::Float);
        structDef->AddField("color", BaseType::Vec4); // w is intensity
        structDef->Finalize();
        return structDef;
    }

    inline std::shared_ptr<ShaderStructDefinition> CreateSpotLightType(
        LayoutStandard standard = LayoutStandard::Std140) {

        auto structDef = std::make_shared<ShaderStructDefinition>("SpotLight", standard);
        structDef->AddField("position", BaseType::Vec3);
        structDef->AddField("innerCutoff", BaseType::Float);
        structDef->AddField("direction", BaseType::Vec3);
        structDef->AddField("outerCutoff", BaseType::Float);
        structDef->AddField("color", BaseType::Vec4); // w is intensity
        structDef->AddField("range", BaseType::Float);
        structDef->AddField("padding", BaseType::Vec3);
        structDef->Finalize();
        return structDef;
    }

    // ============================================================================
    // C++ DATA STRUCTURES (for CPU-side data)
    // ============================================================================

    struct DirectionalLightData {
        glm::vec3 direction;
        float padding1;
        glm::vec4 color;

        void SetDefaults() {
            direction = glm::vec3(0.0f, -1.0f, 0.0f);
            color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            padding1 = 0.0f;
        }
    };

    struct PointLightData {
        glm::vec3 position;
        float radius;
        glm::vec4 color;

        void SetDefaults() {
            position = glm::vec3(0.0f);
            radius = 0.0f;
            color = glm::vec4(0.0f);
        }
    };

    struct SpotLightData {
        glm::vec3 position;
        float innerCutoff;
        glm::vec3 direction;
        float outerCutoff;
        glm::vec4 color;
        float range;
        glm::vec3 padding;

        void SetDefaults() {
            position = glm::vec3(0.0f);
            innerCutoff = 0.0f;
            direction = glm::vec3(0.0f);
            outerCutoff = 0.0f;
            color = glm::vec4(0.0f);
            range = 0.0f;
            padding = glm::vec3(0.0f);
        }
    };

    // ============================================================================
    // HELPER FUNCTIONS - Work with instances
    // ============================================================================

    inline void SetDirectionalLight(std::shared_ptr<ShaderStructInstance> lightInstance,
        const DirectionalLightData& data) {
        (*lightInstance)["direction"] = data.direction;
        (*lightInstance)["color"] = data.color;
    }

    inline DirectionalLightData GetDirectionalLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        DirectionalLightData data;
        data.direction = lightInstance->Get<glm::vec3>("direction");
        data.color = lightInstance->Get<glm::vec4>("color");
        return data;
    }

    inline void SetPointLight(std::shared_ptr<ShaderStructInstance> lightInstance,
        const PointLightData& data) {
        (*lightInstance)["position"] = data.position;
        (*lightInstance)["radius"] = data.radius;
        (*lightInstance)["color"] = data.color;
    }

    inline PointLightData GetPointLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        PointLightData data;
        data.position = lightInstance->Get<glm::vec3>("position");
        data.radius = lightInstance->Get<float>("radius");
        data.color = lightInstance->Get<glm::vec4>("color");
        return data;
    }

    inline void SetSpotLight(std::shared_ptr<ShaderStructInstance> lightInstance,
        const SpotLightData& data) {
        (*lightInstance)["position"] = data.position;
        (*lightInstance)["innerCutoff"] = data.innerCutoff;
        (*lightInstance)["direction"] = data.direction;
        (*lightInstance)["outerCutoff"] = data.outerCutoff;
        (*lightInstance)["color"] = data.color;
        (*lightInstance)["range"] = data.range;
        (*lightInstance)["padding"] = data.padding;
    }

    inline SpotLightData GetSpotLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        SpotLightData data;
        data.position = lightInstance->Get<glm::vec3>("position");
        data.innerCutoff = lightInstance->Get<float>("innerCutoff");
        data.direction = lightInstance->Get<glm::vec3>("direction");
        data.outerCutoff = lightInstance->Get<float>("outerCutoff");
        data.color = lightInstance->Get<glm::vec4>("color");
        data.range = lightInstance->Get<float>("range");
        data.padding = lightInstance->Get<glm::vec3>("padding");
        return data;
    }

} // namespace ShaderLib