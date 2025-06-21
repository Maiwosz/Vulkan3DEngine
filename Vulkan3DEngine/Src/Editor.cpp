#include "Editor.h"
#include "LoggerConfig.h"
#include "Paths.h"
#include <filesystem>
#include <iostream>

Editor::Editor(const std::string& sourceRelative, const std::string& destRelative)
    : m_sourceRelative(sourceRelative)
    , m_destRelative(destRelative)
{
    // Note: Don't use EDITOR_LOG_* macros here yet - logging not initialized
    std::cout << "[EDITOR] Initializing Editor..." << std::endl;

    // Create Engine instance first
    m_engine = std::make_unique<Engine>();

    // Initialize Engine (this will load settings and setup logging system)
    initializeEngine();

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

    // Flush all loggers
    auto editorLogger = LoggerConfig::getNamedLogger("EDITOR");
    auto engineLogger = LoggerConfig::getNamedLogger("ENGINE");

    if (editorLogger) editorLogger->flush();
    if (engineLogger) engineLogger->flush();

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
        std::cout << "[EDITOR] Initializing Engine..." << std::endl;

        Engine::InitParams initParams;
        initParams.title = "Vulkan3DEngine - Editor";
        initParams.logDir = LOGS_DIR;
        initParams.enableThreadPool = true;
        initParams.threadCount = 0; // Auto-detect

        // Engine initialization will load settings and setup logging
        m_engine->initialize(initParams);

        EDITOR_LOG_INFO("Engine initialized successfully");
    }
    catch (const std::exception& e) {
        // Try to log with EDITOR logger if available, otherwise use cout
        auto editorLogger = LoggerConfig::getNamedLogger("EDITOR");
        if (editorLogger) {
            EDITOR_LOG_CRITICAL("Failed to initialize Engine: {}", e.what());
        }
        else {
            std::cerr << "[EDITOR] CRITICAL: Failed to initialize Engine: " << e.what() << std::endl;
        }
        throw;
    }
}