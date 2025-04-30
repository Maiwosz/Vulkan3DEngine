#include "Editor.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "Paths.h"

Editor::Editor(const std::string& sourceRelative, const std::string& destRelative) {
    configureLogger();

    m_assetWatcher = std::make_unique<AssetWatcher>(
        ASSETS_SRC,
        ASSETS_COMP,
        *this
    );
}

Editor::~Editor() {
    stop();
    m_logger->debug("Editor destroyed");
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
        std::filesystem::create_directories("logs");

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/editor.log");
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // IDENTYCZNY FORMAT JAK W SILNIKU
        constexpr const char* ENGINE_FORMAT = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] - %v";

        file_sink->set_pattern(ENGINE_FORMAT);
        console_sink->set_pattern(ENGINE_FORMAT);  // Usuwamy krótszy format czasu

        std::vector<spdlog::sink_ptr> sinks{ file_sink, console_sink };
        m_logger = std::make_shared<spdlog::logger>("EDITOR", sinks.begin(), sinks.end());

        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::err);  // Dopasowujemy do zachowania silnika
    }
    catch (const spdlog::spdlog_ex& ex) {
        SPDLOG_ERROR("Editor logger init failed: {}", ex.what());
        throw;
    }
}