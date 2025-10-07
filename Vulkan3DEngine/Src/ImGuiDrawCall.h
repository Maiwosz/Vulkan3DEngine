#pragma once
#include "GpuCall.h"
#include "GpuCallTypes.h"
#include <functional>

/**
 * GPU call for executing ImGui rendering commands.
 * Contains a callback for ImGui UI definition.
 */
class ImGuiDrawCall : public TypedGpuCall<ImGuiDrawCall> {
public:
    using ImGuiCallback = std::function<void()>;

    explicit ImGuiDrawCall(ImGuiCallback callback)
        : m_callback(std::move(callback)) {
    }

    bool execute(Renderer& renderer, EngineCore& engineCore, RenderNode& renderNode) override;

    // Optional: set callback dynamically
    void setCallback(ImGuiCallback callback) {
        m_callback = std::move(callback);
    }

private:
    ImGuiCallback m_callback;
};