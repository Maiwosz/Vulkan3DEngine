#include "Engine.h"
#include "Editor.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>

void signal_handler(int) {
    SPDLOG_CRITICAL("Critical signal received!");
    spdlog::default_logger()->flush();
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
    spdlog::set_level(spdlog::level::trace);

    SPDLOG_INFO("=== Launching application ===");

    try {
        Engine& engine = Engine::get();
        engine.initialize("Vulkan3DEngine"); // Full logger configuration happens here

        Editor editor("Assets/Source", "Assets/Compiled");
        SPDLOG_DEBUG("Initializing editor...");
        editor.start();

        SPDLOG_INFO("Entering main loop");
        engine.run();

        while(1) {

        }

        SPDLOG_DEBUG("Stopping editor...");
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