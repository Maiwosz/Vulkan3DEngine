#pragma once
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include "BuiltInStructures.h"
#include <glm/glm.hpp>
#include <iostream>

namespace ShaderLib {

    // ============================================================================
    // STANDARD BUFFER FACTORY METHODS
    // ============================================================================

    inline std::shared_ptr<BufferObjectDefinition> CreateGlobalUBODefinition() {
        // Create composite type definitions
        auto directionalLightDef = CreateDirectionalLightType();
        auto pointLightDef = CreatePointLightType();
        auto spotLightDef = CreateSpotLightType();

        // Create structure definition
        auto structDef = std::make_shared<StructureDefinition>("GlobalUBO");
        structDef->AddField("view", BaseType::Mat4)
            .AddField("proj", BaseType::Mat4)
            .AddField("cameraPosition", BaseType::Vec3)
            .AddField("directionalLight", directionalLightDef)
            .AddField("activePointLights", BaseType::Int)
            .AddField("pointLights", pointLightDef, 64)  // Array of 64 point lights
            .AddField("activeSpotLights", BaseType::Int)
            .AddField("spotLights", spotLightDef, 16);   // Array of 16 spot lights

        // Create buffer definition
        auto bufferDef = MakeUniformBuffer(structDef);
        bufferDef->SetUseInstanceName(false);

        return bufferDef;
    }

    inline std::shared_ptr<BufferObjectDefinition> CreateObjectUBODefinition() {
        auto structDef = std::make_shared<StructureDefinition>("ObjectUBO");
        structDef->AddField("model", BaseType::Mat4)
            .AddField("color", BaseType::Vec4);

        auto bufferDef = MakeUniformBuffer(structDef);
        bufferDef->SetUseInstanceName(false);

        return bufferDef;
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
        auto globalUBODef = CreateGlobalUBODefinition();
        auto objectUBODef = CreateObjectUBODefinition();

        uint32_t globalUBOSize = globalUBODef->GetTotalSize();
        uint32_t objectUBOSize = objectUBODef->GetTotalSize();

        if (sizeof(GlobalUBOData) < globalUBOSize) {
            std::cerr << "ERROR: GlobalUBOData struct size (" << sizeof(GlobalUBOData)
                << ") is smaller than GlobalUBO buffer size (" << globalUBOSize << ")" << std::endl;
        }

        if (sizeof(ObjectUBOData) < objectUBOSize) {
            std::cerr << "ERROR: ObjectUBOData struct size (" << sizeof(ObjectUBOData)
                << ") is smaller than ObjectUBO buffer size (" << objectUBOSize << ")" << std::endl;
        }
    }

    // ============================================================================
    // HELPER FUNCTIONS - Convenience wrappers for BufferObjectInstance
    // ============================================================================

    inline void SetGlobalUBOData(std::shared_ptr<BufferObjectInstance> bufferInstance,
        const GlobalUBOData& data) {
        bufferInstance->Set("view", data.view);
        bufferInstance->Set("proj", data.proj);
        bufferInstance->Set("cameraPosition", data.cameraPosition);

        // Set directional light
        auto dirLight = bufferInstance->GetField("directionalLight");
        dirLight["direction"] = data.directionalLight.direction;
        dirLight["color"] = data.directionalLight.color;

        bufferInstance->Set("activePointLights", data.activePointLights);
        bufferInstance->Set("activeSpotLights", data.activeSpotLights);

        // Set point lights array
        for (int i = 0; i < 64; i++) {
            auto pointLight = bufferInstance->GetField("pointLights[" + std::to_string(i) + "]");
            pointLight["position"] = data.pointLights[i].position;
            pointLight["radius"] = data.pointLights[i].radius;
            pointLight["color"] = data.pointLights[i].color;
        }

        // Set spot lights array
        for (int i = 0; i < 16; i++) {
            auto spotLight = bufferInstance->GetField("spotLights[" + std::to_string(i) + "]");
            spotLight["position"] = data.spotLights[i].position;
            spotLight["innerCutoff"] = data.spotLights[i].innerCutoff;
            spotLight["direction"] = data.spotLights[i].direction;
            spotLight["outerCutoff"] = data.spotLights[i].outerCutoff;
            spotLight["color"] = data.spotLights[i].color;
            spotLight["range"] = data.spotLights[i].range;
            spotLight["padding"] = data.spotLights[i].padding;
        }
    }

    inline GlobalUBOData GetGlobalUBOData(std::shared_ptr<const BufferObjectInstance> bufferInstance) {
        GlobalUBOData data;

        data.view = bufferInstance->Get<glm::mat4>("view");
        data.proj = bufferInstance->Get<glm::mat4>("proj");
        data.cameraPosition = bufferInstance->Get<glm::vec3>("cameraPosition");

        // Get directional light
        data.directionalLight.direction = bufferInstance->Get<glm::vec3>("directionalLight.direction");
        data.directionalLight.color = bufferInstance->Get<glm::vec4>("directionalLight.color");

        data.activePointLights = bufferInstance->Get<int32_t>("activePointLights");
        data.activeSpotLights = bufferInstance->Get<int32_t>("activeSpotLights");

        // Get point lights array
        for (int i = 0; i < 64; i++) {
            std::string basePath = "pointLights[" + std::to_string(i) + "]";
            data.pointLights[i].position = bufferInstance->Get<glm::vec3>(basePath + ".position");
            data.pointLights[i].radius = bufferInstance->Get<float>(basePath + ".radius");
            data.pointLights[i].color = bufferInstance->Get<glm::vec4>(basePath + ".color");
        }

        // Get spot lights array
        for (int i = 0; i < 16; i++) {
            std::string basePath = "spotLights[" + std::to_string(i) + "]";
            data.spotLights[i].position = bufferInstance->Get<glm::vec3>(basePath + ".position");
            data.spotLights[i].innerCutoff = bufferInstance->Get<float>(basePath + ".innerCutoff");
            data.spotLights[i].direction = bufferInstance->Get<glm::vec3>(basePath + ".direction");
            data.spotLights[i].outerCutoff = bufferInstance->Get<float>(basePath + ".outerCutoff");
            data.spotLights[i].color = bufferInstance->Get<glm::vec4>(basePath + ".color");
            data.spotLights[i].range = bufferInstance->Get<float>(basePath + ".range");
            data.spotLights[i].padding = bufferInstance->Get<glm::vec3>(basePath + ".padding");
        }

        return data;
    }

    inline void SetObjectUBOData(std::shared_ptr<BufferObjectInstance> bufferInstance,
        const ObjectUBOData& data) {
        bufferInstance->Set("model", data.model);
        bufferInstance->Set("color", data.color);
    }

    inline ObjectUBOData GetObjectUBOData(std::shared_ptr<const BufferObjectInstance> bufferInstance) {
        ObjectUBOData data;
        data.model = bufferInstance->Get<glm::mat4>("model");
        data.color = bufferInstance->Get<glm::vec4>("color");
        return data;
    }

} // namespace ShaderLib
