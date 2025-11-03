#pragma once
#include "ShaderLib.h"
#include "BufferBuilder.h"
#include "BuiltInStructures.h"
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <glm/glm.hpp>
#include <iostream>

namespace ShaderLib {

    // ============================================================================
    // STANDARD BUFFER FACTORY METHODS
    // ============================================================================

    inline BufferObject CreateGlobalUBO() {
        BufferBuilder builder(
            "GlobalUBO",
            BufferType::Uniform,
            BufferAccessMode::ReadOnly,
            LayoutStandard::Std140
        );

        // Create composite type definitions
        auto directionalLightDef = CreateDirectionalLightType();
        auto pointLightDef = CreatePointLightType();
        auto spotLightDef = CreateSpotLightType();

        // Create array definitions
        auto pointLightsArrayDef = std::make_shared<ShaderArrayDefinition>(
            pointLightDef, 64, LayoutStandard::Std140);
        auto spotLightsArrayDef = std::make_shared<ShaderArrayDefinition>(
            spotLightDef, 16, LayoutStandard::Std140);

        builder.AddField("view", BaseType::Mat4)
            .AddField("proj", BaseType::Mat4)
            .AddField("cameraPosition", BaseType::Vec3)
            .AddCompositeField("directionalLight", directionalLightDef)
            .AddField("activePointLights", BaseType::Int)
            .AddCompositeField("pointLights", pointLightsArrayDef)
            .AddField("activeSpotLights", BaseType::Int)
            .AddCompositeField("spotLights", spotLightsArrayDef)
            .SetUseInstanceName(false); // No instance name for built-in buffer

        return builder.Build();
    }

    inline BufferObject CreateObjectUBO() {
        BufferBuilder builder(
            "ObjectUBO",
            BufferType::Uniform,
            BufferAccessMode::ReadOnly,
            LayoutStandard::Std140
        );

        builder.AddField("model", BaseType::Mat4)
            .AddField("color", BaseType::Vec4)
            .SetUseInstanceName(false);  // No instance name for built-in buffer

        return builder.Build();
    }

    // ============================================================================
    // C++ BUFFER DATA STRUCTURES
    // ============================================================================

    struct GlobalUBOData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPosition;
        float padding1;
        DirectionalLightData directionalLight;
        int32_t activePointLights;
        float padding2[3];
        PointLightData pointLights[64];
        int32_t activeSpotLights;
        float padding3[3];
        SpotLightData spotLights[16];

        void SetDefaults() {
            view = glm::mat4(1.0f);
            proj = glm::mat4(1.0f);
            cameraPosition = glm::vec3(0.0f);
            directionalLight.SetDefaults();
            activePointLights = 0;
            activeSpotLights = 0;

            for (int i = 0; i < 64; i++) {
                pointLights[i].SetDefaults();
            }
            for (int i = 0; i < 16; i++) {
                spotLights[i].SetDefaults();
            }
        }
    };

    struct ObjectUBOData {
        glm::mat4 model;
        glm::vec4 color;

        void SetDefaults() {
            model = glm::mat4(1.0f);
            color = glm::vec4(1.0f);
        }
    };

    // ============================================================================
    // BUFFER SIZE VALIDATION
    // ============================================================================

    inline void ValidateBufferStructSizes() {
        BufferObject globalUBO = CreateGlobalUBO();
        BufferObject objectUBO = CreateObjectUBO();

        if (sizeof(GlobalUBOData) < globalUBO.size) {
            std::cerr << "ERROR: GlobalUBOData struct size (" << sizeof(GlobalUBOData)
                << ") is smaller than GlobalUBO.size (" << globalUBO.size << ")" << std::endl;
        }

        if (sizeof(ObjectUBOData) < objectUBO.size) {
            std::cerr << "ERROR: ObjectUBOData struct size (" << sizeof(ObjectUBOData)
                << ") is smaller than ObjectUBO.size (" << objectUBO.size << ")" << std::endl;
        }
    }

} // namespace ShaderLib