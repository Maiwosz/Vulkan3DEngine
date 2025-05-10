#pragma once
#include "ShaderLib.h"
#include "UBODefinitions.h"
#include <glm/glm.hpp>

namespace ShaderLib {

    /**
     * Standard UBO definitions that match those created in UBORegistry::CreateGlobalUBO and CreateObjectUBO
     * These can be used to access the predefined UBO definitions in the registry
     */

     // The GlobalUBO definition as a static variable that matches UBORegistry::CreateGlobalUBO()
    inline static const UniformBufferObject GLOBAL_UBO = []() {
        UBOBuilder builder("GlobalUBO", GLOBAL_DESCRIPTOR_SET, 0);

        // Add common global variables
        builder.AddField<glm::mat4>("view", "View matrix")
            .AddField<glm::mat4>("proj", "Projection matrix")
            .AddField<glm::vec3>("cameraPosition", "Camera position in world space")

            // Add directional light
            .AddStructField<DirectionalLight>("directionalLight", "Main directional light")

            // Add point lights array
            .AddField<int>("activePointLights", "Number of active point lights")
            .AddArrayField<PointLight>("pointLights", 64, "Array of point lights")

            // Add spot lights array
            .AddField<int>("activeSpotLights", "Number of active spot lights")
            .AddArrayField<SpotLight>("spotLights", 16, "Array of spot lights");

        return builder.Build();
        }();

    // The ObjectUBO definition as a static variable that matches UBORegistry::CreateObjectUBO()
    inline static const UniformBufferObject OBJECT_UBO = []() {
        UBOBuilder builder("ObjectUBO", OBJECT_DESCRIPTOR_SET, 0);

        // Add object-specific variables
        builder.AddField<glm::mat4>("model", "Model matrix")
            .AddField<glm::vec4>("color", "Object color/tint");

        return builder.Build();
        }();

    /**
     * C++ struct templates for GlobalUBO and ObjectUBO with proper alignment
     * These structs MUST match the layout and structure defined in
     * UBORegistry::CreateGlobalUBO() and UBORegistry::CreateObjectUBO()
     */

     // Global UBO struct template - matching the definition in UBORegistry::CreateGlobalUBO()
    struct GlobalUBOData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 cameraPosition;
        float padding1;         // Dopełnienie dla wyrównania następnej struktury
        DirectionalLight directionalLight;
        int activePointLights;
        float padding2[3];      // Dopełnienie dla wyrównania do 16 bajtów
        PointLight pointLights[64];
        int activeSpotLights;
        float padding3[3];      // Dopełnienie dla wyrównania do 16 bajtów
        SpotLight spotLights[16];

        // Helper method to initialize the UBO with default values
        void SetDefaults() {
            view = glm::mat4(1.0f);
            proj = glm::mat4(1.0f);
            cameraPosition = glm::vec3(0.0f);

            // Initialize directional light
            directionalLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
            directionalLight.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // Clear light counters
            activePointLights = 0;
            activeSpotLights = 0;

            // Thoroughly initialize light arrays
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
                // Initialize padding if necessary
                spotLights[i].padding[0] = 0.0f;
                spotLights[i].padding[1] = 0.0f;
                spotLights[i].padding[2] = 0.0f;
            }
        }
    };

    // NOTE: You should verify at runtime that sizeof(GlobalUBOData) >= GLOBAL_UBO.size
    // Runtime validation can be done with: assert(sizeof(GlobalUBOData) >= GLOBAL_UBO.size);

    // Object UBO struct template - matching the definition in UBORegistry::CreateObjectUBO()
    struct ObjectUBOData {
        glm::mat4 model;
        glm::vec4 color;

        // Helper method to initialize the UBO with default values
        void SetDefaults() {
            model = glm::mat4(1.0f);      // Identity matrix
            color = glm::vec4(1.0f);      // White with full opacity
        }
    };

    // NOTE: You should verify at runtime that sizeof(ObjectUBOData) >= OBJECT_UBO.size
    // Runtime validation can be done with: assert(sizeof(ObjectUBOData) >= OBJECT_UBO.size);

    // Helper function to validate UBO struct sizes at runtime
    inline void ValidateUBOStructSizes() {
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