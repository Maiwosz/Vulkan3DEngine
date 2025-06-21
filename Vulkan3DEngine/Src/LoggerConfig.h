#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <unordered_map>

class LoggerConfig {
public:
    struct LoggerOptions {
        std::string loggerName = "APP";
        std::string logDir = "logs/";
        std::string filePrefix = "app_";
        std::string fileExtension = ".log";
        spdlog::level::level_enum level = spdlog::level::info;
        bool enableConsole = true;
        bool enableFile = true;
        bool timestampFilename = true;
        bool rotateOldLogs = true;
        size_t maxOldLogFiles = 2;
        std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] - %v";
    };

    // Create logger with separate file but shared console
    static std::shared_ptr<spdlog::logger> createLogger(const LoggerOptions& options);

    // Create a multi-sink logger that writes to both shared console and separate file
    static std::shared_ptr<spdlog::logger> createSharedConsoleLogger(
        const LoggerOptions& options,
        std::shared_ptr<spdlog::sinks::sink> sharedConsoleSink = nullptr
    );

    // Get or create shared console sink
    static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> getSharedConsoleSink();

    // Setup global logging with multiple named loggers
    static void setupGlobalLogging(const std::string& logDir, spdlog::level::level_enum level = spdlog::level::info);

    // Get named logger (creates if doesn't exist)
    static std::shared_ptr<spdlog::logger> getNamedLogger(const std::string& name);

private:
    static std::string generateLogFilename(const LoggerOptions& options);
    static void rotateLogFiles(const LoggerOptions& options);

    // Shared console sink for all loggers
    static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> s_sharedConsoleSink;
    static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> s_namedLoggers;
};

// Convenient macros for different components
#define ENGINE_LOG_TRACE(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->trace(__VA_ARGS__); } while(0)
#define ENGINE_LOG_DEBUG(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->debug(__VA_ARGS__); } while(0)
#define ENGINE_LOG_INFO(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->info(__VA_ARGS__); } while(0)
#define ENGINE_LOG_WARN(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->warn(__VA_ARGS__); } while(0)
#define ENGINE_LOG_ERROR(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->error(__VA_ARGS__); } while(0)
#define ENGINE_LOG_CRITICAL(...) do { auto logger = LoggerConfig::getNamedLogger("ENGINE"); if(logger) logger->critical(__VA_ARGS__); } while(0)

#define EDITOR_LOG_TRACE(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->trace(__VA_ARGS__); } while(0)
#define EDITOR_LOG_DEBUG(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->debug(__VA_ARGS__); } while(0)
#define EDITOR_LOG_INFO(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->info(__VA_ARGS__); } while(0)
#define EDITOR_LOG_WARN(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->warn(__VA_ARGS__); } while(0)
#define EDITOR_LOG_ERROR(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->error(__VA_ARGS__); } while(0)
#define EDITOR_LOG_CRITICAL(...) do { auto logger = LoggerConfig::getNamedLogger("EDITOR"); if(logger) logger->critical(__VA_ARGS__); } while(0)