#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "Renderer.h"
#include "Scene.h"
#include "ThreadPool.h"
#include "AssetSystem.h"
#include "InputSystem.h"
#include "RenderSystem.h"

class Scene;

class Engine
{
public:
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    static Engine& get() {
        static Engine instance;
        return instance;
    }

    void initialize(const char* title = "Engine");
    void shutdown();

    void update();
    void run();

    float getDeltaTime() const { return m_deltaTime; }
    float getTotalTime() const { return m_totalTime; }
    uint32_t getFrameCount() const { return m_frameCount; }

    Settings& settings() const { return *m_settings; }
    Window& window() const { return *m_window; }
    ThreadPool& threadPool() const { return *m_threadPool; }
    InputSystem& inputSystem() const { return *m_inputSystem; }
    Renderer& renderer() const { return *m_renderer; }
    Scene& scene() const { return *m_scene; }
    AssetSystem& assetSystem() const { return *m_assetSystem; } 
    AssetManager& assetManager() const { return m_assetSystem->assetManager(); }
    RenderSystem& renderSystem() const { return *m_renderSystem; }

private:
    Engine() = default;
    ~Engine() = default;

    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<InputSystem> m_inputSystem;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<AssetSystem> m_assetSystem;
    std::unique_ptr<RenderSystem> m_renderSystem;

    bool m_running = false;
    float m_lastFrameTime = 0.0f;
    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;
    uint32_t m_frameCount = 0;
};