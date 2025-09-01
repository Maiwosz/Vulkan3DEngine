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
class Renderer;
class AssetSystem;

enum class RenderOrderType {
    Mesh,
    Light,
    Camera,
    EditorUI
};

// Convert RenderOrderType to string for logging purposes
inline std::string renderOrderTypeToString(RenderOrderType type) {
    switch (type) {
    case RenderOrderType::Mesh: return "Mesh";
    case RenderOrderType::Light: return "Light";
    case RenderOrderType::Camera: return "Camera";
    case RenderOrderType::EditorUI: return "EditorUI";
    default: return "Unknown";
    }
}

class RenderOrder {
public:
    virtual ~RenderOrder() = default;

    // Type identification
    virtual RenderOrderType getType() const = 0;

    // Polymorphic execution - each render order type implements its own rendering logic
    virtual void execute(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) {
        // Default implementation for unsupported render order types
        SPDLOG_ERROR("RenderOrder::execute() called for unsupported type: {}",
            renderOrderTypeToString(getType()));
    }

    Entity entity;
};