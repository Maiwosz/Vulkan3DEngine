#pragma once
#include "ShaderStruct.h"
#include <glm/glm.hpp>
#include <memory>

namespace ShaderLib {

    // ============================================================================
    // LIGHT STRUCTURE FACTORIES
    // ============================================================================

    inline std::shared_ptr<ShaderStruct> CreateDirectionalLightType(LayoutStandard standard = LayoutStandard::Std140) {
        auto structType = std::make_shared<ShaderStruct>("DirectionalLight", standard);
        structType->AddField<glm::vec3>("direction");
        structType->AddField<glm::vec4>("color"); // w is intensity
        structType->Finalize();
        return structType;
    }

    inline std::shared_ptr<ShaderStruct> CreatePointLightType(LayoutStandard standard = LayoutStandard::Std140) {
        auto structType = std::make_shared<ShaderStruct>("PointLight", standard);
        structType->AddField<glm::vec3>("position");
        structType->AddField<float>("radius");
        structType->AddField<glm::vec4>("color"); // w is intensity
        structType->Finalize();
        return structType;
    }

    inline std::shared_ptr<ShaderStruct> CreateSpotLightType(LayoutStandard standard = LayoutStandard::Std140) {
        auto structType = std::make_shared<ShaderStruct>("SpotLight", standard);
        structType->AddField<glm::vec3>("position");
        structType->AddField<float>("innerCutoff");
        structType->AddField<glm::vec3>("direction");
        structType->AddField<float>("outerCutoff");
        structType->AddField<glm::vec4>("color"); // w is intensity
        structType->AddField<float>("range");
        structType->AddField<glm::vec3>("padding");
        structType->Finalize();
        return structType;
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
    // HELPER FUNCTIONS
    // ============================================================================

    inline void SetDirectionalLight(std::shared_ptr<ShaderStruct> lightStruct, const DirectionalLightData& data) {
        lightStruct->SetField("direction", data.direction);
        lightStruct->SetField("color", data.color);
    }

    inline DirectionalLightData GetDirectionalLight(std::shared_ptr<ShaderStruct> lightStruct) {
        DirectionalLightData data;
        data.direction = lightStruct->GetField<glm::vec3>("direction");
        data.color = lightStruct->GetField<glm::vec4>("color");
        return data;
    }

    inline void SetPointLight(std::shared_ptr<ShaderStruct> lightStruct, const PointLightData& data) {
        lightStruct->SetField("position", data.position);
        lightStruct->SetField("radius", data.radius);
        lightStruct->SetField("color", data.color);
    }

    inline PointLightData GetPointLight(std::shared_ptr<ShaderStruct> lightStruct) {
        PointLightData data;
        data.position = lightStruct->GetField<glm::vec3>("position");
        data.radius = lightStruct->GetField<float>("radius");
        data.color = lightStruct->GetField<glm::vec4>("color");
        return data;
    }

    inline void SetSpotLight(std::shared_ptr<ShaderStruct> lightStruct, const SpotLightData& data) {
        lightStruct->SetField("position", data.position);
        lightStruct->SetField("innerCutoff", data.innerCutoff);
        lightStruct->SetField("direction", data.direction);
        lightStruct->SetField("outerCutoff", data.outerCutoff);
        lightStruct->SetField("color", data.color);
        lightStruct->SetField("range", data.range);
        lightStruct->SetField("padding", data.padding);
    }

    inline SpotLightData GetSpotLight(std::shared_ptr<ShaderStruct> lightStruct) {
        SpotLightData data;
        data.position = lightStruct->GetField<glm::vec3>("position");
        data.innerCutoff = lightStruct->GetField<float>("innerCutoff");
        data.direction = lightStruct->GetField<glm::vec3>("direction");
        data.outerCutoff = lightStruct->GetField<float>("outerCutoff");
        data.color = lightStruct->GetField<glm::vec4>("color");
        data.range = lightStruct->GetField<float>("range");
        data.padding = lightStruct->GetField<glm::vec3>("padding");
        return data;
    }

} // namespace ShaderLib