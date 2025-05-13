#include "LoggerConfig.h"
#include <algorithm>

std::shared_ptr<spdlog::logger> LoggerConfig::createLogger(const LoggerOptions& options) {
    try {
        // Ensure log directory exists
        std::filesystem::create_directories(options.logDir);

        std::vector<spdlog::sink_ptr> sinks;

        // Add console sink if enabled
        if (options.enableConsole) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(options.pattern);
            sinks.push_back(console_sink);
        }

        // Add file sink if enabled
        if (options.enableFile) {
            // Handle log rotation if needed
            if (options.rotateOldLogs) {
                rotateLogFiles(options);
            }

            // Generate log filename
            std::string log_filename = generateLogFilename(options);

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_filename);
            file_sink->set_pattern(options.pattern);
            sinks.push_back(file_sink);
        }

        // Create logger with all sinks
        auto logger = std::make_shared<spdlog::logger>(options.loggerName, sinks.begin(), sinks.end());

        // Configure log level
        logger->set_level(options.level);
        logger->flush_on(spdlog::level::err);

        return logger;
    }
    catch (const spdlog::spdlog_ex& ex) {
        throw std::runtime_error(std::string("Logger configuration failed: ") + ex.what());
    }
}

std::string LoggerConfig::generateLogFilename(const LoggerOptions& options) {
    std::stringstream ss;
    ss << options.logDir << options.filePrefix;

    if (options.timestampFilename) {
        // Generate timestamp
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);

        // Use platform-specific localtime function
#ifdef _WIN32
        struct tm timeinfo;
        localtime_s(&timeinfo, &now_c);
        ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
#else
        struct tm timeinfo;
        localtime_r(&now_c, &timeinfo);
        ss << std::put_time(&timeinfo, "%Y%m%d_%H%M%S");
#endif
    }

    ss << options.fileExtension;
    return ss.str();
}

void LoggerConfig::rotateLogFiles(const LoggerOptions& options) {
    if (!options.rotateOldLogs || options.maxOldLogFiles == 0) {
        return;
    }

    // Find all log files with the specified prefix
    std::vector<std::filesystem::path> log_files;
    for (const auto& entry : std::filesystem::directory_iterator(options.logDir)) {
        if (entry.is_regular_file() &&
            entry.path().filename().string().starts_with(options.filePrefix) &&
            entry.path().extension() == options.fileExtension) {
            log_files.push_back(entry.path());
        }
    }

    // Sort files by last modification time (oldest first)
    std::sort(log_files.begin(), log_files.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });

    // Remove oldest files, keeping only the specified number of recent logs
    // (plus one for the new log we're about to create)
    while (log_files.size() > options.maxOldLogFiles) {
        std::filesystem::remove(log_files.front());
        log_files.erase(log_files.begin());
    }
}