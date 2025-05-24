#pragma once
#include "Entity.h"
#include "AssetHandle.h"
#include "TransformComponent.h"
#include <memory>
#include <variant>
#include "Handle.h"
#include "ISmartHandleManager.h"
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

// Forward declarations
class Buffer;

enum class RenderOrderType {
    Mesh,
    Light,
    Camera
};

// Convert RenderOrderType to string for logging purposes
inline std::string renderOrderTypeToString(RenderOrderType type) {
    switch (type) {
    case RenderOrderType::Mesh: return "Mesh";
    case RenderOrderType::Light: return "Light";
    case RenderOrderType::Camera: return "Camera";
    default: return "Unknown";
    }
}

class RenderOrder {
public:
    virtual ~RenderOrder() = default;
    virtual RenderOrderType getType() const = 0;
    Entity entity;
};

class MeshRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Mesh; }

    // Asset resolution stage
    MeshHandle meshHandle;
    MaterialHandle materialHandle;

    // Uniform buffer stage - używamy SmartHandle dla automatycznego zarządzania
    SmartHandle<UniformBufferHandle, Buffer> globalUBOHandle;
    SmartHandle<UniformBufferHandle, Buffer> objectUBOHandle;

    // Descriptor sets stage - używamy SmartHandle dla automatycznego zarządzania  
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> globalDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> objectDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> materialDescriptorSetHandle;

    // Pipeline assignment stage
    PipelineHandle pipelineHandle;

    // Metody pomocnicze do sprawdzania ważności zasobów
    bool hasValidGlobalUBO() const { return globalUBOHandle.isValid(); }
    bool hasValidObjectUBO() const { return objectUBOHandle.isValid(); }
    bool hasValidGlobalDescriptorSet() const { return globalDescriptorSetHandle.isValid(); }
    bool hasValidObjectDescriptorSet() const { return objectDescriptorSetHandle.isValid(); }
    bool hasValidMaterialDescriptorSet() const { return materialDescriptorSetHandle.isValid(); }

    // Metoda do sprawdzenia czy wszystkie krytyczne zasoby są dostępne
    bool isReadyForRendering() const {
        return meshHandle.isValid() &&
            materialHandle.isValid() &&
            pipelineHandle.isValid() &&
            hasValidGlobalUBO() &&
            hasValidObjectUBO();
    }
};

class LightRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Light; }
};

class CameraRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Camera; }
};