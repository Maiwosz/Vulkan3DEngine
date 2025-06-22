#include "Engine.h"
#include "LoggerManager.h"
#include "Paths.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <thread>
#include <stdexcept>
#include "Scene.h"

Engine::~Engine() {
    if (m_initialized) {
        shutdown();
    }
}

void Engine::initialize(const InitParams& params) {
    if (m_initialized) {
        SPDLOG_WARN("Engine already initialized");
        return;
    }

    try {
        // Load settings first to get log level
        SPDLOG_INFO("Loading settings from '{}'", params.settingsFile);
        m_settings = std::make_unique<Settings>();
        if (!m_settings->load(params.settingsFile)) {
            SPDLOG_WARN("Failed to load settings, using defaults");
        }

        // Setup logging system with settings-based log level
        initializeLogging(params.logDir, m_settings->getLogLevel());

        SPDLOG_INFO("Initializing Engine...");
        SPDLOG_INFO("Log level set to: {}", Settings::toString(m_settings->getLogLevel()));

        initializeComponents(params);
        connectEventHandlers();

        m_initialized = true;
        SPDLOG_INFO("Engine initialization complete");
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Engine initialization failed: {}", e.what());
        cleanupComponents();
        throw;
    }
}

void Engine::run() {
    if (!m_initialized) {
        throw std::runtime_error("Engine not initialized");
    }

    if (m_running) {
        SPDLOG_WARN("Engine already running");
        return;
    }

    SPDLOG_INFO("Starting main loop");

    m_running = true;
    m_lastFrameTime = std::chrono::high_resolution_clock::now();

    try {
        while (m_running && !m_window->shouldClose()) {
            updateTiming();
            update();
            updateFPS();
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error in main loop: {}", e.what());
        m_running = false;
        throw;
    }

    SPDLOG_INFO("Main loop ended after {} frames", m_frameCount);
}

void Engine::shutdown() {
    if (!m_initialized) {
        return;
    }

    SPDLOG_INFO("Shutting down Engine...");

    m_running = false;
    m_shutdownRequested = true;

    // Wait for renderer to finish current frame
    if (m_renderer) {
        m_renderer->waitIdle();
    }

    // Cleanup in reverse order of initialization
    cleanupComponents();

    m_initialized = false;
    SPDLOG_INFO("Engine shutdown complete");

    // Flush loggers but don't shutdown the logger manager
    // (Editor might still need it)
    if (m_loggerManager) {
        auto engineLogger = m_loggerManager->getNamedLogger("ENGINE");
        if (engineLogger) {
            engineLogger->flush();
        }
    }
}

void Engine::initializeLogging(const std::string& logDir, Settings::LogLevel logLevel) {
    std::filesystem::create_directories(logDir);

    // Convert settings log level to spdlog level
    spdlog::level::level_enum spdlogLevel = Settings::toSpdlogLevel(logLevel);

    // Create logger manager and initialize ENGINE logging
    m_loggerManager = std::make_shared<LoggerManager>();
    m_loggerManager->initializeEngineLogging(logDir, spdlogLevel);

    // Set global logger manager for macro access
    LoggerAccess::setLoggerManager(m_loggerManager);

    // The ENGINE logger is now set as default, so SPDLOG_* macros work
    SPDLOG_INFO("Engine logging initialized with level: {}", Settings::toString(logLevel));
}

void Engine::initializeComponents(const InitParams& params) {
    // Window
    SPDLOG_DEBUG("Creating window");
    Window::CreateInfo windowInfo;
    windowInfo.title = params.title;
    windowInfo.mode = m_settings->getWindowMode();
    windowInfo.resolution = m_settings->getResolution();
    m_window = std::make_unique<Window>(windowInfo);

    // Thread pool
    if (params.enableThreadPool) {
        size_t threadCount = params.threadCount;
        if (threadCount == 0) {
            threadCount = std::thread::hardware_concurrency();
        }
        SPDLOG_DEBUG("Creating thread pool with {} threads", threadCount);
        m_threadPool = std::make_unique<ThreadPool>(threadCount);
    }

    // Input system
    SPDLOG_DEBUG("Initializing input system");
    m_inputSystem = std::make_unique<InputSystem>(*m_window);

    // Renderer
    SPDLOG_DEBUG("Creating renderer");
    m_renderer = std::make_unique<Renderer>(*m_settings, *m_window);

    // Asset system
    SPDLOG_DEBUG("Initializing asset system");
    m_assetSystem = std::make_unique<AssetSystem>(*m_renderer);

    SPDLOG_DEBUG("Creating registry");
    m_registry = std::make_unique<Registry>(*this);

    // Scene
    SPDLOG_DEBUG("Creating scene");
    m_scene = std::make_unique<Scene>(*this, *m_registry);

    // Render system
    SPDLOG_DEBUG("Initializing render system");
    m_renderSystem = std::make_unique<RenderSystem>(
        *m_registry,
        *m_assetSystem,
        *m_renderer,
        *m_settings
    );
}

void Engine::connectEventHandlers() {
    // Connect window close event to shutdown
    m_windowCloseSubscription = m_window->onClose([this]() {
        SPDLOG_INFO("Window close requested");
        requestShutdown();
        });

    // Connect window to settings for automatic updates
    m_window->connectToSettings(*m_settings);
}

void Engine::update() {
    SPDLOG_TRACE("Frame {}: Delta={:.3f}ms", m_frameCount, m_deltaTime * 1000.0f);

    // Poll window events
    m_window->pollEvents();

    // Update input
    m_inputSystem->update();

    // Update scene
    m_scene->update();

    // Process rendering
    try {
        m_renderSystem->processOrders();
        m_renderSystem->renderFrame();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Render error: {}", e.what());
        throw;
    }

    // Advance asset system frame
    m_assetSystem->advanceFrame();

    m_frameCount++;
}

void Engine::updateTiming() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        currentTime - m_lastFrameTime);

    m_deltaTime = duration.count() / 1000000.0f;
    m_totalTime += m_deltaTime;
    m_lastFrameTime = currentTime;
}

void Engine::updateFPS() {
    m_fpsUpdateTimer += m_deltaTime;
    m_fpsFrameCount++;

    if (m_fpsUpdateTimer >= FPS_UPDATE_INTERVAL) {
        m_fps = m_fpsFrameCount / m_fpsUpdateTimer;
        m_fpsUpdateTimer = 0.0f;
        m_fpsFrameCount = 0;
    }
}

void Engine::cleanupComponents() {
    // Unsubscribe from events first
    m_windowCloseSubscription.unsubscribe();

    // Cleanup in reverse order
    m_renderSystem.reset();
    m_scene.reset();
    m_assetSystem.reset();
    m_registry.reset();
    m_renderer.reset();
    m_inputSystem.reset();
    m_window.reset();
    m_threadPool.reset();
    m_settings.reset();
}