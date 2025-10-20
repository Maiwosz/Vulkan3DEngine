#pragma once
#include "ShaderLib.h"
#include "BufferDefinitions.h"
#include <glm/glm.hpp>

namespace ShaderLib {

    /**
     * Standard buffer definitions (UBO and SSBO)
     * These match the buffers created in BufferRegistry
     */

     // GlobalUBO definition - matches BufferRegistry::CreateGlobalUBO()
    inline static const BufferObject GLOBAL_UBO = []() {
        BufferBuilder builder("GlobalUBO", GLOBAL_DESCRIPTOR_SET, 0, BufferType::Uniform, LayoutStandard::Std140);

        builder.AddField<glm::mat4>("view", "View matrix")
            .AddField<glm::mat4>("proj", "Projection matrix")
            .AddField<glm::vec3>("cameraPosition", "Camera position in world space")
            .AddStructField<DirectionalLight>("directionalLight", "Main directional light")
            .AddField<int>("activePointLights", "Number of active point lights")
            .AddArrayField<PointLight>("pointLights", 64, "Array of point lights")
            .AddField<int>("activeSpotLights", "Number of active spot lights")
            .AddArrayField<SpotLight>("spotLights", 16, "Array of spot lights");

        return builder.Build();
        }();

    // ObjectUBO definition - matches BufferRegistry::CreateObjectUBO()
    inline static const BufferObject OBJECT_UBO = []() {
        BufferBuilder builder("ObjectUBO", OBJECT_DESCRIPTOR_SET, 0, BufferType::Uniform, LayoutStandard::Std140);

        builder.AddField<glm::mat4>("model", "Model matrix")
            .AddField<glm::vec4>("color", "Object color/tint");

        return builder.Build();
        }();

    /**
     * C++ struct templates with proper alignment
     * These structs MUST match the layout and structure defined in BufferRegistry
     */

     // Global UBO struct template
    struct GlobalUBOData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPosition;
        float padding1;
        DirectionalLight directionalLight;
        int activePointLights;
        float padding2[3];
        PointLight pointLights[64];
        int activeSpotLights;
        float padding3[3];
        SpotLight spotLights[16];

        void SetDefaults() {
            view = glm::mat4(1.0f);
            proj = glm::mat4(1.0f);
            cameraPosition = glm::vec3(0.0f);

            directionalLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
            directionalLight.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            activePointLights = 0;
            activeSpotLights = 0;

            for (int i = 0; i < 64; i++) {
                pointLights[i].position = glm::vec3(0.0f);
                pointLights[i].radius = 0.0f;
                pointLights[i].color = glm::vec4(0.0f);
            }

            for (int i = 0; i < 16; i++) {
                spotLights[i].position = glm::vec3(0.0f);
                spotLights[i].direction = glm::vec3(0.0f);
                spotLights[i].innerCutoff = 0.0f;
                spotLights[i].outerCutoff = 0.0f;
                spotLights[i].color = glm::vec4(0.0f);
                spotLights[i].range = 0.0f;
                spotLights[i].padding[0] = 0.0f;
                spotLights[i].padding[1] = 0.0f;
                spotLights[i].padding[2] = 0.0f;
            }
        }
    };

    // Object UBO struct template
    struct ObjectUBOData {
        glm::mat4 model;
        glm::vec4 color;

        void SetDefaults() {
            model = glm::mat4(1.0f);
            color = glm::vec4(1.0f);
        }
    };

    // Helper function to validate buffer struct sizes at runtime
    inline void ValidateBufferStructSizes() {
        // Check GlobalUBO size
        if (sizeof(GlobalUBOData) < GLOBAL_UBO.size) {
            std::cerr << "ERROR: GlobalUBOData struct size (" << sizeof(GlobalUBOData)
                << ") is smaller than GLOBAL_UBO.size (" << GLOBAL_UBO.size << ")" << std::endl;
        }

        // Check ObjectUBO size
        if (sizeof(ObjectUBOData) < OBJECT_UBO.size) {
            std::cerr << "ERROR: ObjectUBOData struct size (" << sizeof(ObjectUBOData)
                << ") is smaller than OBJECT_UBO.size (" << OBJECT_UBO.size << ")" << std::endl;
        }
    }

} // namespace ShaderLib