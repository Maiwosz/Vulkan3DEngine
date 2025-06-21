#pragma once
#include "Prerequisites.h"
#include "Settings.h"
#include "Window.h"
#include "InputSystem.h"
#include "Renderer.h"
#include "ThreadPool.h"
#include "AssetSystem.h"
#include "RenderSystem.h"
#include "Scene.h"
#include <memory>
#include <chrono>

class Window;
class Settings;
class InputSystem;
class Renderer;
class ThreadPool;
class AssetSystem;
class RenderSystem;
class Scene;
class Registry;

class Engine {
public:
    struct InitParams {
        std::string title = "Vulkan3DEngine";
        std::string settingsFile = "settings.json";
        std::string logDir = "Logs";
        bool enableThreadPool = true;
        size_t threadCount = 0; // 0 = auto-detect
    };

    // Constructor and destructor
    Engine() = default;
    ~Engine();

    // Non-copyable, non-movable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Core lifecycle
    void initialize(const InitParams& params = {});
    void run();
    void shutdown();

    // Component access
    Settings& settings() const { return *m_settings; }
    Window& window() const { return *m_window; }
    InputSystem& inputSystem() const { return *m_inputSystem; }
    Renderer& renderer() const { return *m_renderer; }
    Scene& scene() const { return *m_scene; }
    ThreadPool& threadPool() const { return *m_threadPool; }
    AssetSystem& assetSystem() const { return *m_assetSystem; }
    RenderSystem& renderSystem() const { return *m_renderSystem; }
    Registry& registry() const { return *m_registry; }

    // Time and frame info
    float deltaTime() const { return m_deltaTime; }
    float totalTime() const { return m_totalTime; }
    uint64_t frameCount() const { return m_frameCount; }
    float fps() const { return m_fps; }

    // State queries
    bool isRunning() const { return m_running; }
    bool isInitialized() const { return m_initialized; }

    // Control
    void requestShutdown() { m_running = false; }

private:
    // Initialization helpers
    void initializeLogging(const std::string& logDir, Settings::LogLevel logLevel);
    void initializeComponents(const InitParams& params);
    void connectEventHandlers();

    // Main loop
    void update();
    void updateTiming();
    void updateFPS();

    // Cleanup
    void cleanupComponents();

private:
    // Core components - order matters for destruction
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<InputSystem> m_inputSystem;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Registry> m_registry;
    std::unique_ptr<AssetSystem> m_assetSystem;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<RenderSystem> m_renderSystem;

    // State
    bool m_initialized = false;
    bool m_running = false;
    bool m_shutdownRequested = false;

    // Timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;
    uint64_t m_frameCount = 0;

    // FPS tracking
    float m_fps = 0.0f;
    float m_fpsUpdateTimer = 0.0f;
    uint32_t m_fpsFrameCount = 0;
    static constexpr float FPS_UPDATE_INTERVAL = 1.0f; // Update FPS every second

    // Event subscriptions for cleanup
    Window::CloseEvent::Subscription m_windowCloseSubscription;
};