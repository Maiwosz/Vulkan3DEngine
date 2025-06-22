#include "Editor.h"
#include "LoggerManager.h"
#include "Paths.h"
#include <filesystem>
#include <iostream>

Editor::Editor(const std::string& sourceRelative, const std::string& destRelative)
    : m_sourceRelative(sourceRelative)
    , m_destRelative(destRelative)
{
    // Note: Don't use EDITOR_LOG_* macros here yet - logging not initialized
    SPDLOG_INFO("Initializing Editor...");

    // Create Engine instance first
    m_engine = std::make_unique<Engine>();

    // Initialize Engine (this will load settings and setup logging system)
    initializeEngine();

    // Get shared ownership of LoggerManager from Engine
    m_loggerManager = m_engine->getLoggerManager();
    if (!m_loggerManager) {
        SPDLOG_CRITICAL("Failed to get LoggerManager from Engine");
        throw std::runtime_error("LoggerManager not available from Engine");
    }

    // Initialize Editor's own logger
    m_loggerManager->addEditorLogger(LOGS_DIR);

    // Now we can use proper logging
    EDITOR_LOG_INFO("Engine initialized, continuing Editor setup...");

    // Create AssetWatcher
    m_assetWatcher = std::make_unique<AssetWatcher>(
        sourceRelative,
        destRelative,
        *this
    );

    // Perform initial asset scan
    EDITOR_LOG_INFO("Performing initial asset scan...");
    m_assetWatcher->Run();
    EDITOR_LOG_INFO("Initial asset scan completed");

    // Initialize EditorUI with engine reference
    m_editorUI = std::make_unique<EditorUI>(*m_engine);

    m_engine->renderSystem().renderStage().setUIRenderCallback(
        [this]() {
            m_editorUI->renderWindows();
        }
    );

    EDITOR_LOG_INFO("Editor initialized successfully");
}

Editor::~Editor() {
    stop();

    // Shutdown engine if it's still running
    if (m_engine && m_engine->isInitialized()) {
        EDITOR_LOG_INFO("Shutting down engine from Editor destructor");
        m_engine->shutdown();
    }

    // Flush all loggers if LoggerManager is still available
    if (m_loggerManager) {
        auto editorLogger = m_loggerManager->getNamedLogger("EDITOR");
        auto engineLogger = m_loggerManager->getNamedLogger("ENGINE");

        if (editorLogger) editorLogger->flush();
        if (engineLogger) engineLogger->flush();

        // Release our reference to LoggerManager
        m_loggerManager.reset();
    }

    // Shutdown spdlog completely
    spdlog::shutdown();
}

void Editor::start() {
    EDITOR_LOG_INFO("Starting editor");
    m_running = true;

    // Start asset watcher thread
    m_watcherThread = std::thread([this]() {
        EDITOR_LOG_DEBUG("Asset watcher thread started");
        while (m_running) {
            m_assetWatcher->Run();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        EDITOR_LOG_DEBUG("Asset watcher thread exiting");
        });

    // Run engine main loop
    if (m_engine && m_engine->isInitialized()) {
        EDITOR_LOG_INFO("Starting engine main loop");
        m_engine->run();
    }
}

void Editor::stop() {
    if (m_running) {
        EDITOR_LOG_INFO("Stopping editor...");
        m_running = false;

        // Stop engine first
        if (m_engine && m_engine->isRunning()) {
            EDITOR_LOG_DEBUG("Requesting engine shutdown");
            m_engine->requestShutdown();
        }

        // Wait for asset watcher thread
        if (m_watcherThread.joinable()) {
            m_watcherThread.join();
        }
    }
}

void Editor::initializeEngine() {
    try {
        SPDLOG_INFO("Initializing Engine...");

        Engine::InitParams initParams;
        initParams.title = "Vulkan3DEngine - Editor";
        initParams.logDir = LOGS_DIR;
        initParams.enableThreadPool = true;
        initParams.threadCount = 0; // Auto-detect

        // Engine initialization will load settings and setup logging
        m_engine->initialize(initParams);

        SPDLOG_INFO("Engine initialized successfully");
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Failed to initialize Engine: {}", e.what());
        throw;
    }
}