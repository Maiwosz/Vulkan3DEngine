#include "Engine.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void Engine::initialize(const char* title) {
    SPDLOG_TRACE("Starting engine initialization");

    // Ensure the logs directory exists
    std::filesystem::create_directories("logs");

    // Load settings
    m_settings = std::make_unique<Settings>();
    if (!m_settings->loadFromFile("settings.json")) {
        SPDLOG_WARN("Failed to load settings, using default configuration");
    }
    // Configure logging system
    try {
        // Sink dla pliku
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/engine.log");

        // Sink dla konsoli (kolorowy)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // Łączymy sinki w jeden logger
        std::vector<spdlog::sink_ptr> sinks{ file_sink, console_sink };
        auto engine_logger = std::make_shared<spdlog::logger>("ENGINE", sinks.begin(), sinks.end());

        // Formatowanie (dla wszystkich sinków)
        engine_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] - %v");

        engine_logger->set_level(Settings::convertLogLevel(m_settings->getLogLevel()));
        engine_logger->flush_on(spdlog::level::err);
        spdlog::set_default_logger(engine_logger);
        SPDLOG_INFO("Logger configured (level: {})", spdlog::level::to_string_view(Settings::convertLogLevel(m_settings->getLogLevel())));

        // Subskrypcja zmiany poziomu logów
        m_logLevelChangedSubscription = std::make_unique<Event<Settings::LogLevel>::Subscription>(
            m_settings->onLogLevelChanged()->subscribe([](Settings::LogLevel level) {
                SPDLOG_INFO("Log level changed to: {}", Settings::LOG_LEVEL_MAP.at(level));
                spdlog::set_level(Settings::convertLogLevel(level));
                })
        );
    }
    catch (const spdlog::spdlog_ex& ex) {
        SPDLOG_ERROR("Logger configuration failed: {}", ex.what());
        throw std::runtime_error("Failed to initialize logger");
    }

    // Initialize components
    SPDLOG_DEBUG("Creating window: {}x{}", m_settings->getResolution().width, m_settings->getResolution().height);
    m_window = std::make_unique<Window>(title, m_settings->getWindowMode(), m_settings->getResolution());

    SPDLOG_INFO("Initializing thread pool ({} workers)", std::thread::hardware_concurrency());
    m_threadPool = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());

    SPDLOG_DEBUG("Initializing input system");
    m_inputSystem = std::make_unique<InputSystem>(*m_window);

    SPDLOG_INFO("Creating renderer");
    m_renderer = std::make_unique<Renderer>(*m_window);

    SPDLOG_DEBUG("Initializing asset manager");
    m_assetManager = std::make_unique<AssetManager>(
        m_renderer->getVramManager(),
        m_renderer->getShaderModuleManager(),
		m_renderer->getMaterialManager()
    );

    SPDLOG_INFO("Preparing scene");
    m_scene = std::make_unique<Scene>();

    SPDLOG_INFO("Initializing Render System");
    m_renderSystem = std::make_unique<RenderSystem>(
        m_scene->registry(),
        *m_assetManager 
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
    m_renderer->drawFrame();
    m_assetManager->advanceFrame();
}

void Engine::run() {
    m_running = true;
    m_lastFrameTime = static_cast<float>(glfwGetTime());

    SPDLOG_INFO("Starting main loop");
    try {
        while (m_running && !m_window->shouldClose()) {
            const float currentTime = static_cast<float>(glfwGetTime());
            m_deltaTime = currentTime - m_lastFrameTime;
            m_lastFrameTime = currentTime;
            m_totalTime += m_deltaTime;
            m_frameCount++;

            SPDLOG_TRACE("Frame {} (Delta: {:.3f}ms)", m_frameCount, m_deltaTime * 1000.0f);
            update();
        }
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Critical error in main loop: {}", e.what());
        shutdown();
        throw;
    }

    SPDLOG_INFO("Main loop terminated (processed {} frames)", m_frameCount);
}