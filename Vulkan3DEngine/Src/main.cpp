#include "Editor.h"
#include "LoggerConfig.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include "Paths.h"

void signal_handler(int) {
    // Use named loggers for signal handling
    auto engineLogger = LoggerConfig::getNamedLogger("ENGINE");
    auto editorLogger = LoggerConfig::getNamedLogger("EDITOR");

    if (engineLogger) {
        engineLogger->critical("Critical signal received!");
        engineLogger->flush();
    }

    if (editorLogger) {
        editorLogger->critical("Critical signal received!");
        editorLogger->flush();
    }

    spdlog::shutdown();
    std::exit(EXIT_FAILURE);
}

int main() {
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);

    // Basic console logging for pre-initialization phase
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto preinit_logger = std::make_shared<spdlog::logger>("PREINIT", console_sink);
    spdlog::set_default_logger(preinit_logger);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    preinit_logger->set_level(spdlog::level::trace);

    SPDLOG_INFO("=== Launching application ===");

    try {
        // Create and initialize editor (which will create and initialize engine internally)
        // The engine initialization will setup the proper logging system
        SPDLOG_INFO("Creating Editor...");
        Editor editor(ASSETS_SRC, ASSETS_COMP);

        // Start editor (this will run the engine main loop)
        SPDLOG_INFO("Starting Editor...");
        editor.start();

        // When we get here, the application is shutting down
        EDITOR_LOG_INFO("Editor finished, stopping...");
        editor.stop();
    }
    catch (const std::exception& e) {
        SPDLOG_CRITICAL("Critical error: {}", e.what());
        spdlog::default_logger()->flush();
        return EXIT_FAILURE;
    }

    SPDLOG_INFO("Application shutdown complete");
    return EXIT_SUCCESS;
}