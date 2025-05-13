#include "Engine.h"
#include "LoggerConfig.h"
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>

void Engine::initialize(const char* title) {
    std::filesystem::create_directories(LOGS_DIR);

    m_settings = std::make_unique<Settings>();
    if (!m_settings->loadFromFile("settings.json")) {
        // Na tym etapie logger jeszcze nie jest skonfigurowany
        std::cerr << "Ostrzeżenie: Nie udało się załadować ustawień, używam domyślnej konfiguracji" << std::endl;
    }

    try {
        LoggerConfig::LoggerOptions options;
        options.loggerName = "ENGINE";
        options.logDir = LOGS_DIR;
        options.filePrefix = "engine_";
        options.level = Settings::convertLogLevel(m_settings->getLogLevel());
        options.maxOldLogFiles = 2;  // zachowaj 2 stare pliki + nowy

        auto logger = LoggerConfig::createLogger(options);
        spdlog::set_default_logger(logger);

        SPDLOG_INFO("Logger configured (level: {})",
            spdlog::level::to_string_view(Settings::convertLogLevel(m_settings->getLogLevel())));
    }
    catch (const std::exception& ex) {
        std::cerr << "Logger configuration failed: " << ex.what() << std::endl;
        throw std::runtime_error("Failed to initialize logger");
    }

    SPDLOG_TRACE("Engine initialization started");

    // Initialize components
    SPDLOG_DEBUG("Creating window: {}x{}",
        m_settings->getCurrentResolutionDetails().width,
        m_settings->getCurrentResolutionDetails().height);
    m_window = std::make_unique<Window>(title, m_settings->getWindowMode(), m_settings->getResolution());

    SPDLOG_INFO("Initializing thread pool ({} workers)", std::thread::hardware_concurrency());
    m_threadPool = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());

    SPDLOG_DEBUG("Initializing input system");
    m_inputSystem = std::make_unique<InputSystem>(*m_window);

    SPDLOG_INFO("Creating renderer");
    m_renderer = std::make_unique<Renderer>(*m_window);

    SPDLOG_DEBUG("Initializing asset manager");
    m_assetManager = std::make_unique<AssetManager>(
        m_renderer->vramManager(),
        m_renderer->shaderModuleManager(),
        m_renderer->materialManager(),
        m_renderer->meshManager()
    );

    SPDLOG_INFO("Preparing scene");
    m_scene = std::make_unique<Scene>();

    SPDLOG_INFO("Initializing Render System");
    m_renderSystem = std::make_unique<RenderSystem>(
        m_scene->registry(),
        *m_assetManager,
        *m_renderer
    );

    SPDLOG_INFO("=== Engine ready ===");
}

void Engine::shutdown() {
    SPDLOG_INFO("Initiating engine shutdown");

    SPDLOG_DEBUG("Destroying scene...");
    m_scene.reset();

    SPDLOG_DEBUG("Cleaning up asset manager...");
    m_assetManager.reset();

    SPDLOG_INFO("Shutting down renderer...");
    m_renderer.reset();

    SPDLOG_DEBUG("Destroying input system...");
    m_inputSystem.reset();

    SPDLOG_DEBUG("Stopping thread pool...");
    m_threadPool.reset();

    SPDLOG_DEBUG("Closing window...");
    m_window.reset();

    SPDLOG_INFO("=== Engine shutdown complete ===");
    spdlog::default_logger()->flush();
    spdlog::shutdown();
}

void Engine::update() {
    m_window->pollEvents();
    m_inputSystem->update();
    m_scene->update();

    try {
        m_renderSystem->processOrders();
        m_renderSystem->renderFrame();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Render error: {}", e.what());
    }

    //m_assetManager->advanceFrame(); //tymczasowe wyłącznie przez problemy
}

void Engine::run() {
    m_running = true;
    m_lastFrameTime = static_cast<float>(glfwGetTime());

    SPDLOG_INFO("Starting main loop");
    try {
        while (m_running && !m_window->shouldClose()) {
            try {
                const float currentTime = static_cast<float>(glfwGetTime());
                m_deltaTime = currentTime - m_lastFrameTime;
                m_lastFrameTime = currentTime;
                m_totalTime += m_deltaTime;
                m_frameCount++;

                SPDLOG_TRACE("Frame {} (Delta: {:.3f}ms)", m_frameCount, m_deltaTime * 1000.0f);
                update();
            }
            catch (const std::exception& e) {
                SPDLOG_ERROR("Error in frame {}: {}", m_frameCount, e.what());
                m_renderer->waitIdle();
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Critical error in main loop: {}", e.what());
        shutdown();
        throw;
    }

    SPDLOG_INFO("Main loop terminated (processed {} frames)", m_frameCount);
}