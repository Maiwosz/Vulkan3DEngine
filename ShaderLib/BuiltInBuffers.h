#pragma once
#include "ShaderLib.h"
#include "BufferBuilder.h"
#include "BuiltInStructures.h"
#include "ShaderArray.h"
#include <glm/glm.hpp>
#include <iostream>

namespace ShaderLib {

    // ============================================================================
    // STANDARD BUFFER FACTORY METHODS
    // ============================================================================

    inline BufferObject CreateGlobalUBO() {
        BufferBuilder builder("GlobalUBO", GLOBAL_DESCRIPTOR_SET, 0, BufferType::Uniform, LayoutStandard::Std140);

        builder.AddField<glm::mat4>("view")
            .AddField<glm::mat4>("proj")
            .AddField<glm::vec3>("cameraPosition")
            .AddCompositeField("directionalLight", CreateDirectionalLightType())
            .AddField<int32_t>("activePointLights")
            .AddCompositeField("pointLights", std::make_shared<ShaderArray>(CreatePointLightType(), 64))
            .AddField<int32_t>("activeSpotLights")
            .AddCompositeField("spotLights", std::make_shared<ShaderArray>(CreateSpotLightType(), 16));

        return builder.Build();
    }

    inline BufferObject CreateObjectUBO() {
        BufferBuilder builder("ObjectUBO", OBJECT_DESCRIPTOR_SET, 0, BufferType::Uniform, LayoutStandard::Std140);

        builder.AddField<glm::mat4>("model")
            .AddField<glm::vec4>("color");

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