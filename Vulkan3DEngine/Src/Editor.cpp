#include "Editor.h"
#include "LoggerConfig.h"
#include "Paths.h"
#include <filesystem>
#include <iostream>

Editor::Editor(const std::string& sourceRelative, const std::string& destRelative) {
    configureLogger();

    m_assetWatcher = std::make_unique<AssetWatcher>(
        sourceRelative,
        destRelative,
        *this
    );
}

Editor::~Editor() {
    stop();
    if (m_logger) {
        m_logger->flush();
    }
}

void Editor::start() {
    m_logger->info("Starting editor");
    m_running = true;
    m_watcherThread = std::thread([this]() {
        m_logger->debug("Asset watcher thread started");
        while (m_running) {
            m_assetWatcher->Run();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        m_logger->debug("Asset watcher thread exiting");
        });
}

void Editor::stop() {
    if (m_running) {
        m_logger->info("Stopping editor...");
        m_running = false;
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