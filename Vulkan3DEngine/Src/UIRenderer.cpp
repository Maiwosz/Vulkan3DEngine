#include "UIRenderer.h"
#include "Renderer.h"
#include "ImGuiWrapper.h"
#include <algorithm>
#include <stdexcept>
#include <imgui.h>

UIRenderer::UIRenderer(Renderer& renderer, ImGuiWrapper& imguiWrapper)
    : m_renderer(renderer), m_imguiWrapper(imguiWrapper) {
    // Reserve some capacity to avoid frequent reallocations
    m_callbacks.reserve(16);
}

UIRenderer::CallbackId UIRenderer::addCallback(UICallback callback, uint32_t priority) {
    if (!callback) {
        throw std::invalid_argument("UIRenderer: Cannot add null callback");
    }

    // Clamp priority to valid range
    uint32_t clampedPriority = clampPriority(priority);

    // Create new entry
    CallbackEntry entry;
    entry.id = m_nextId++;
    entry.callback = std::move(callback);
    entry.priority = clampedPriority;

    m_callbacks.push_back(std::move(entry));
    m_needsSort = true;

    return entry.id;
}

bool UIRenderer::removeCallback(CallbackId callbackId) {
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
        [callbackId](const CallbackEntry& entry) {
            return entry.id == callbackId;
        });

    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
        return true;
    }

    return false;
}

void UIRenderer::removeAllCallbacks() {
    m_callbacks.clear();
    m_needsSort = false;
}

bool UIRenderer::hasCallbacks() const {
    return !m_callbacks.empty();
}

void UIRenderer::render() {
    if (m_callbacks.empty()) {
        return;
    }

    // Start ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Sort callbacks by priority if needed (priority 1 = highest)
    if (m_needsSort) {
        sortCallbacks();
        m_needsSort = false;
    }

    // Execute all callbacks in priority order
    for (const auto& entry : m_callbacks) {
        try {
            entry.callback();
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("UIRenderer: Exception in UI callback (ID: {}): {}", entry.id, e.what());
        }
        catch (...) {
            SPDLOG_ERROR("UIRenderer: Unknown exception in UI callback (ID: {})", entry.id);
        }
    }

    // Use ImGuiWrapper to render
    m_imguiWrapper.render(m_renderer.getCurrentCommandBuffer());
}

uint32_t UIRenderer::clampPriority(uint32_t priority) const {
    if (priority < HIGHEST_PRIORITY) {
        return HIGHEST_PRIORITY;
    }
    if (priority > LOWEST_PRIORITY) {
        return LOWEST_PRIORITY;
    }
    return priority;
}

void UIRenderer::sortCallbacks() {
    std::sort(m_callbacks.begin(), m_callbacks.end());
}