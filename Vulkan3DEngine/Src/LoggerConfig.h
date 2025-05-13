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
        size_t maxOldLogFiles = 2;  // Number of old log files to keep (excluding current)
        std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] - %v";
    };

    static std::shared_ptr<spdlog::logger> createLogger(const LoggerOptions& options);

private:
    static std::string generateLogFilename(const LoggerOptions& options);
    static void rotateLogFiles(const LoggerOptions& options);
};