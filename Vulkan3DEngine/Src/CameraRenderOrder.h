#pragma once
#include "RenderOrder.h"

class CameraRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Camera; }

    // Uses default implementation from RenderOrder which logs error
    // Camera orders should not reach the execution stage
};