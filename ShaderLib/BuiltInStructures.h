#pragma once
#include "StructureDefinition.h"
#include <glm/glm.hpp>
#include <memory>

namespace ShaderLib {

    // ============================================================================
    // LIGHT STRUCTURE DEFINITION FACTORIES
    // ============================================================================

    inline std::shared_ptr<StructureDefinition> CreateDirectionalLightType() {
        auto structDef = std::make_shared<StructureDefinition>("DirectionalLight");
        structDef->AddField("direction", BaseType::Vec3)
            .AddField("color", BaseType::Vec4);  // w is intensity
        return structDef;
    }

    inline std::shared_ptr<StructureDefinition> CreatePointLightType() {
        auto structDef = std::make_shared<StructureDefinition>("PointLight");
        structDef->AddField("position", BaseType::Vec3)
            .AddField("radius", BaseType::Float)
            .AddField("color", BaseType::Vec4);  // w is intensity
        return structDef;
    }

    inline std::shared_ptr<StructureDefinition> CreateSpotLightType() {
        auto structDef = std::make_shared<StructureDefinition>("SpotLight");
        structDef->AddField("position", BaseType::Vec3)
            .AddField("innerCutoff", BaseType::Float)
            .AddField("direction", BaseType::Vec3)
            .AddField("outerCutoff", BaseType::Float)
            .AddField("color", BaseType::Vec4)  // w is intensity
            .AddField("range", BaseType::Float)
            .AddField("padding", BaseType::Vec3);
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

} // namespace ShaderLib
