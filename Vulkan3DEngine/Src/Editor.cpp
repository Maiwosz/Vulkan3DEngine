#include "Editor.h"
#include "LoggerConfig.h"
#include "Paths.h"
#include <filesystem>
#include <iostream>

Editor::Editor(const std::string& sourceRelative, const std::string& destRelative)
    : m_sourceRelative(sourceRelative)
    , m_destRelative(destRelative)
{
    // Configure logger first
    configureLogger();

    m_logger->info("Initializing Editor...");

    // Create AssetWatcher
    m_assetWatcher = std::make_unique<AssetWatcher>(
        sourceRelative,
        destRelative,
        *this
    );

    // Perform initial asset scan
    SPDLOG_INFO("Performing initial asset scan...");
    m_assetWatcher->Run();
    SPDLOG_INFO("Initial asset scan completed");

    // Create Engine instance
    m_engine = std::make_unique<Engine>();

    // Initialize Engine
    initializeEngine();

    // Initialize EditorUI with engine reference
    m_editorUI = std::make_unique<EditorUI>(*m_engine);

    m_engine->renderSystem().renderStage().setUIRenderCallback(
        [this]() {
            m_editorUI->renderWindows();
        }
    );

    m_logger->info("Editor initialized successfully");
}

Editor::~Editor() {
    stop();

    // Shutdown engine if it's still running
    if (m_engine && m_engine->isInitialized()) {
        m_logger->info("Shutting down engine from Editor destructor");
        m_engine->shutdown();
    }

    if (m_logger) {
        m_logger->flush();
    }
}

void Editor::start() {
    m_logger->info("Starting editor");
    m_running = true;

    // Start asset watcher thread
    m_watcherThread = std::thread([this]() {
        m_logger->debug("Asset watcher thread started");
        while (m_running) {
            m_assetWatcher->Run();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        m_logger->debug("Asset watcher thread exiting");
        });

    // Run engine main loop
    if (m_engine && m_engine->isInitialized()) {
        m_logger->info("Starting engine main loop");
        m_engine->run();
    }
}

void Editor::stop() {
    if (m_running) {
        m_logger->info("Stopping editor...");
        m_running = false;

        // Stop engine first
        if (m_engine && m_engine->isRunning()) {
            m_logger->debug("Requesting engine shutdown");
            m_engine->requestShutdown();
        }

        // Wait for asset watcher thread
        if (m_watcherThread.joinable()) {
            m_watcherThread.join();
        }
    }
}

void Editor::configureLogger() {
    try {
        LoggerConfig::LoggerOptions options;
        options.loggerName = "EDITOR";
        options.logDir = LOGS_DIR;
        options.filePrefix = "editor_";
        options.level = spdlog::level::debug;
        options.maxOldLogFiles = 2;

        m_logger = LoggerConfig::createLogger(options);

        m_logger->info("Editor logger configured (level: {})",
            spdlog::level::to_string_view(m_logger->level()));
    }
    catch (const std::exception& ex) {
        std::cerr << "Editor logger init failed: " << ex.what() << std::endl;
        throw std::runtime_error("Failed to initialize logger");
    }
}

void Editor::initializeEngine() {
    try {
        m_logger->info("Initializing Engine...");

        Engine::InitParams initParams;
        initParams.title = "Vulkan3DEngine - Editor";
        initParams.logDir = LOGS_DIR;
        initParams.enableThreadPool = true;
        initParams.threadCount = 0; // Auto-detect

        m_engine->initialize(initParams);

        m_logger->info("Engine initialized successfully");
    }
    catch (const std::exception& e) {
        m_logger->critical("Failed to initialize Engine: {}", e.what());
        throw;
    }
}