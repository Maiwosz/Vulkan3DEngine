#pragma once
#include "Prerequisites.h"
#include <vector>
#include <functional>
#include <algorithm>
#include <limits>

class Renderer;
class ImGuiWrapper;

/**
 * UIRenderer - manages UI rendering callbacks with priority system
 *
 * Provides interface for registering UI callbacks from multiple engine systems
 * Priority 1 is highest, values are clamped to valid range
 */
class UIRenderer {
public:
    using UICallback = std::function<void()>;
    using CallbackId = uint32_t;

    // Priority constants
    static constexpr uint32_t HIGHEST_PRIORITY = 1;
    static constexpr uint32_t LOWEST_PRIORITY = 1000;
    static constexpr uint32_t DEFAULT_PRIORITY = 500;

    explicit UIRenderer(Renderer& renderer, ImGuiWrapper& imguiWrapper);
    ~UIRenderer() = default;

    // Callback management
    CallbackId addCallback(UICallback callback, uint32_t priority = DEFAULT_PRIORITY);
    bool removeCallback(CallbackId callbackId);
    void removeAllCallbacks();

    // Rendering interface (used by Renderer)
    bool hasCallbacks() const;
    void render();

    // Utility
    size_t getCallbackCount() const { return m_callbacks.size(); }

private:
    struct CallbackEntry {
        CallbackId id;
        UICallback callback;
        uint32_t priority;

        // Sort by priority (1 = highest priority first)
        bool operator<(const CallbackEntry& other) const {
            return priority < other.priority;
        }
    };

    Renderer& m_renderer;
    ImGuiWrapper& m_imguiWrapper;
    std::vector<CallbackEntry> m_callbacks;
    CallbackId m_nextId = 1;
    bool m_needsSort = false;

    // Helpers
    uint32_t clampPriority(uint32_t priority) const;
    void sortCallbacks();
};