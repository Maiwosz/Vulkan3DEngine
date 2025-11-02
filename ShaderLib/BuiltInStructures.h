#pragma once
#include "ShaderStruct.h"
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
        lightInstance->SetField("direction", data.direction);
        lightInstance->SetField("color", data.color);
    }

    inline DirectionalLightData GetDirectionalLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        DirectionalLightData data;
        data.direction = std::get<glm::vec3>(lightInstance->GetField("direction"));
        data.color = std::get<glm::vec4>(lightInstance->GetField("color"));
        return data;
    }

    inline void SetPointLight(std::shared_ptr<ShaderStructInstance> lightInstance,
        const PointLightData& data) {
        lightInstance->SetField("position", data.position);
        lightInstance->SetField("radius", data.radius);
        lightInstance->SetField("color", data.color);
    }

    inline PointLightData GetPointLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        PointLightData data;
        data.position = std::get<glm::vec3>(lightInstance->GetField("position"));
        data.radius = std::get<float>(lightInstance->GetField("radius"));
        data.color = std::get<glm::vec4>(lightInstance->GetField("color"));
        return data;
    }

    inline void SetSpotLight(std::shared_ptr<ShaderStructInstance> lightInstance,
        const SpotLightData& data) {
        lightInstance->SetField("position", data.position);
        lightInstance->SetField("innerCutoff", data.innerCutoff);
        lightInstance->SetField("direction", data.direction);
        lightInstance->SetField("outerCutoff", data.outerCutoff);
        lightInstance->SetField("color", data.color);
        lightInstance->SetField("range", data.range);
        lightInstance->SetField("padding", data.padding);
    }

    inline SpotLightData GetSpotLight(std::shared_ptr<ShaderStructInstance> lightInstance) {
        SpotLightData data;
        data.position = std::get<glm::vec3>(lightInstance->GetField("position"));
        data.innerCutoff = std::get<float>(lightInstance->GetField("innerCutoff"));
        data.direction = std::get<glm::vec3>(lightInstance->GetField("direction"));
        data.outerCutoff = std::get<float>(lightInstance->GetField("outerCutoff"));
        data.color = std::get<glm::vec4>(lightInstance->GetField("color"));
        data.range = std::get<float>(lightInstance->GetField("range"));
        data.padding = std::get<glm::vec3>(lightInstance->GetField("padding"));
        return data;
    }

} // namespace ShaderLib