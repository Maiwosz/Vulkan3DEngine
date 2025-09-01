#pragma once
#include "RenderOrder.h"

class LightRenderOrder : public RenderOrder {
public:
    RenderOrderType getType() const override { return RenderOrderType::Light; }

    // Uses default implementation from RenderOrder which logs error
    // Light orders should not reach the execution stage
};