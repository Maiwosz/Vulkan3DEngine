#pragma once
#include "Entity.h"
#include "AssetHandle.h"
#include "TransformComponent.h"
#include <memory>
#include <variant>
#include "MeshHandle.h"
#include "MaterialHandle.h"
#include "UniformBufferHandle.h"
#include <vulkan/vulkan.h>
#include "PipelineHandle.h"
#include <spdlog/spdlog.h>

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

    // Uniform buffer stage
    UniformBufferHandle globalUBOHandle;
    UniformBufferHandle objectUBOHandle;
    UniformBufferHandle materialUBOHandle;

    // Descriptor sets stage
    VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet objectDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;

    // Pipeline assignment stage
    PipelineHandle pipelineHandle;
};

class LightRenderOrder : public RenderOrder {
public:
    enum class LightType { Directional, Point };

    RenderOrderType getType() const override { return RenderOrderType::Light; }

    // Added for logging purposes
    LightType lightType;

    // Convert LightType to string for logging
    std::string getLightTypeString() const {
        switch (lightType) {
        case LightType::Directional: return "Directional";
        case LightType::Point: return "Point";
        default: return "Unknown";
        }
    }

    // Default constructor explicitly setting light type
    LightRenderOrder() : lightType(LightType::Point) {
        SPDLOG_TRACE("Created LightRenderOrder with default Point type");
    }

    // Constructor with explicit light type
    explicit LightRenderOrder(LightType type) : lightType(type) {
        SPDLOG_TRACE("Created LightRenderOrder with explicit type: {}", getLightTypeString());
    }
};

class CameraRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Camera; }
};