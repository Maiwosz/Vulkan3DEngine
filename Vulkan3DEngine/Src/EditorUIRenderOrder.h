#pragma once
#include "RenderOrder.h"
#include <functional>

class EditorUIRenderOrder : public RenderOrder {
public:
    using UICallback = std::function<void()>;

    EditorUIRenderOrder(UICallback callback = nullptr)
        : m_callback(callback) {
    }

    RenderOrderType getType() const override { return RenderOrderType::EditorUI; }

    void execute(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetSystem) override;

    void setCallback(UICallback callback) { m_callback = callback; }
    bool hasCallback() const { return m_callback != nullptr; }

private:
    UICallback m_callback;
};