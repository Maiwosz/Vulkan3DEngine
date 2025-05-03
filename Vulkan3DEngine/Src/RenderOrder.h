#pragma once
#include "Entity.h"
#include "AssetHandle.h"
#include "TransformComponent.h"
#include <memory>
#include <variant>
#include "VramMesh.h"
#include "MaterialHandle.h"

enum class RenderOrderType {
    Mesh,
    Light,
    Camera
};

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
    VramMesh vramMeshHandle;
    MaterialHandle materialHandle;
};

class LightRenderOrder : public RenderOrder {
public:
    enum class LightType { Directional, Point };

    RenderOrderType getType() const override { return RenderOrderType::Light; }

};

class CameraRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Camera; }
    
};