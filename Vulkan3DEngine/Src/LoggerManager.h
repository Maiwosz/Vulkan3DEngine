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

class LoggerManager {
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

    LoggerManager() = default;
    ~LoggerManager();

    // Initialize with ENGINE logger only (called by Engine)
    void initializeEngineLogging(const std::string& logDir, spdlog::level::level_enum level = spdlog::level::info);

    // Add EDITOR logger (called by Editor after taking shared ownership)
    void addEditorLogger(const std::string& logDir, spdlog::level::level_enum level = spdlog::level::info);

    // Create logger with separate file but shared console
    std::shared_ptr<spdlog::logger> createLogger(const LoggerOptions& options);

    // Create a multi-sink logger that writes to both shared console and separate file
    std::shared_ptr<spdlog::logger> createSharedConsoleLogger(
        const LoggerOptions& options,
        std::shared_ptr<spdlog::sinks::sink> sharedConsoleSink = nullptr
    );

    // Get or create shared console sink
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> getSharedConsoleSink();

    // Get named logger (creates if doesn't exist)
    std::shared_ptr<spdlog::logger> getNamedLogger(const std::string& name);

    // Shutdown all loggers safely
    void shutdown();

    // Check if logger exists
    bool hasLogger(const std::string& name) const;

private:
    std::string generateLogFilename(const LoggerOptions& options);
    void rotateLogFiles(const LoggerOptions& options);

    // Shared console sink for all loggers
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> m_sharedConsoleSink;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_namedLoggers;
    bool m_isShutdown = false;
};

// Global instance access
class LoggerAccess {
public:
    // Set the global logger manager (called by Engine)
    static void setLoggerManager(std::shared_ptr<LoggerManager> manager);

    // Get the global logger manager
    static std::shared_ptr<LoggerManager> getLoggerManager();

    // Get named logger through global manager
    static std::shared_ptr<spdlog::logger> getNamedLogger(const std::string& name);

private:
    static std::weak_ptr<LoggerManager> s_loggerManager;
};

// Convenient macros for different components
#define ENGINE_LOG_TRACE(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->trace(__VA_ARGS__); } while(0)
#define ENGINE_LOG_DEBUG(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->debug(__VA_ARGS__); } while(0)
#define ENGINE_LOG_INFO(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->info(__VA_ARGS__); } while(0)
#define ENGINE_LOG_WARN(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->warn(__VA_ARGS__); } while(0)
#define ENGINE_LOG_ERROR(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->error(__VA_ARGS__); } while(0)
#define ENGINE_LOG_CRITICAL(...) do { auto logger = LoggerAccess::getNamedLogger("ENGINE"); if(logger) logger->critical(__VA_ARGS__); } while(0)

#define EDITOR_LOG_TRACE(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->trace(__VA_ARGS__); } while(0)
#define EDITOR_LOG_DEBUG(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->debug(__VA_ARGS__); } while(0)
#define EDITOR_LOG_INFO(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->info(__VA_ARGS__); } while(0)
#define EDITOR_LOG_WARN(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->warn(__VA_ARGS__); } while(0)
#define EDITOR_LOG_ERROR(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->error(__VA_ARGS__); } while(0)
#define EDITOR_LOG_CRITICAL(...) do { auto logger = LoggerAccess::getNamedLogger("EDITOR"); if(logger) logger->critical(__VA_ARGS__); } while(0)