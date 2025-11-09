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
    // VALIDATION HELPERS
    // ============================================================================

    inline void ValidateObjectUBOLayout() {
        auto bufferDef = CreateObjectUBODefinition();
        const auto* layout = bufferDef->GetLayout().get();

        // Validate total size
        if (sizeof(ObjectUBOData) != layout->GetTotalSize()) {
            throw std::runtime_error(
                "ObjectUBOData size mismatch! "
                "C++ struct: " + std::to_string(sizeof(ObjectUBOData)) + " bytes, "
                "GLSL layout: " + std::to_string(layout->GetTotalSize()) + " bytes"
            );
        }

        // Validate field offsets
        const auto* modelField = layout->FindField("model");
        const auto* colorField = layout->FindField("color");

        if (!modelField || !colorField) {
            throw std::runtime_error("Required fields not found in ObjectUBO layout");
        }

        if (offsetof(ObjectUBOData, model) != modelField->offset) {
            throw std::runtime_error(
                "ObjectUBOData::model offset mismatch! "
                "C++ offset: " + std::to_string(offsetof(ObjectUBOData, model)) + ", "
                "Layout offset: " + std::to_string(modelField->offset)
            );
        }

        if (offsetof(ObjectUBOData, color) != colorField->offset) {
            throw std::runtime_error(
                "ObjectUBOData::color offset mismatch! "
                "C++ offset: " + std::to_string(offsetof(ObjectUBOData, color)) + ", "
                "Layout offset: " + std::to_string(colorField->offset)
            );
        }

        // Validate field sizes
        if (sizeof(ObjectUBOData::model) != modelField->size) {
            throw std::runtime_error("ObjectUBOData::model size mismatch");
        }

        if (sizeof(ObjectUBOData::color) != colorField->size) {
            throw std::runtime_error("ObjectUBOData::color size mismatch");
        }
    }

    inline void ValidateGlobalUBOLayout() {
        auto bufferDef = CreateGlobalUBODefinition();
        const auto* layout = bufferDef->GetLayout().get();

        // Validate total size
        if (sizeof(GlobalUBOData) < layout->GetTotalSize()) {
            throw std::runtime_error(
                "GlobalUBOData size too small! "
                "C++ struct: " + std::to_string(sizeof(GlobalUBOData)) + " bytes, "
                "GLSL layout: " + std::to_string(layout->GetTotalSize()) + " bytes"
            );
        }

        // Validate critical field offsets
        const auto* viewField = layout->FindField("view");
        const auto* projField = layout->FindField("proj");
        const auto* cameraPosField = layout->FindField("cameraPosition");
        const auto* dirLightField = layout->FindField("directionalLight");
        const auto* activePointLightsField = layout->FindField("activePointLights");
        const auto* pointLightsField = layout->FindField("pointLights[0]");

        if (!viewField || !projField || !cameraPosField || !dirLightField ||
            !activePointLightsField || !pointLightsField) {
            throw std::runtime_error("Required fields not found in GlobalUBO layout");
        }

        // Validate offsets
        if (offsetof(GlobalUBOData, view) != viewField->offset) {
            throw std::runtime_error(
                "GlobalUBOData::view offset mismatch! "
                "C++ offset: " + std::to_string(offsetof(GlobalUBOData, view)) + ", "
                "Layout offset: " + std::to_string(viewField->offset)
            );
        }

        if (offsetof(GlobalUBOData, proj) != projField->offset) {
            throw std::runtime_error("GlobalUBOData::proj offset mismatch");
        }

        if (offsetof(GlobalUBOData, cameraPosition) != cameraPosField->offset) {
            throw std::runtime_error("GlobalUBOData::cameraPosition offset mismatch");
        }

        if (offsetof(GlobalUBOData, directionalLight) != dirLightField->offset) {
            throw std::runtime_error("GlobalUBOData::directionalLight offset mismatch");
        }

        if (offsetof(GlobalUBOData, activePointLights) != activePointLightsField->offset) {
            throw std::runtime_error("GlobalUBOData::activePointLights offset mismatch");
        }

        if (offsetof(GlobalUBOData, pointLights) != pointLightsField->offset) {
            throw std::runtime_error("GlobalUBOData::pointLights offset mismatch");
        }

        // Validate array stride
        if (sizeof(PointLightData) != pointLightsField->stride) {
            throw std::runtime_error(
                "PointLightData stride mismatch! "
                "C++ size: " + std::to_string(sizeof(PointLightData)) + ", "
                "Layout stride: " + std::to_string(pointLightsField->stride)
            );
        }
    }

    // ============================================================================
    // HELPER FUNCTIONS - Convenience wrappers for BufferObjectInstance
    // ============================================================================

} // namespace ShaderLib
