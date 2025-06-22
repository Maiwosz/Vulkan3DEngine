#include "LoggerManager.h"
#include <algorithm>

// Static member definitions for LoggerAccess
std::weak_ptr<LoggerManager> LoggerAccess::s_loggerManager;

LoggerManager::~LoggerManager() {
    if (!m_isShutdown) {
        shutdown();
    }
}

void LoggerManager::initializeEngineLogging(const std::string& logDir, spdlog::level::level_enum level) {
    std::filesystem::create_directories(logDir);

    // Create shared console sink
    auto consoleSink = getSharedConsoleSink();

    // Create ENGINE logger
    LoggerOptions engineOptions;
    engineOptions.loggerName = "ENGINE";
    engineOptions.logDir = logDir;
    engineOptions.filePrefix = "engine_";
    engineOptions.level = level;
    engineOptions.maxOldLogFiles = 2;

    auto engineLogger = createSharedConsoleLogger(engineOptions, consoleSink);
    m_namedLoggers["ENGINE"] = engineLogger;

    // Set ENGINE logger as default for SPDLOG macros
    spdlog::set_default_logger(engineLogger);
}

void LoggerManager::addEditorLogger(const std::string& logDir, spdlog::level::level_enum level) {
    if (m_isShutdown) {
        return;
    }

    std::filesystem::create_directories(logDir);

    // Get shared console sink
    auto consoleSink = getSharedConsoleSink();

    // Create EDITOR logger
    LoggerOptions editorOptions;
    editorOptions.loggerName = "EDITOR";
    editorOptions.logDir = logDir;
    editorOptions.filePrefix = "editor_";
    editorOptions.level = level;
    editorOptions.maxOldLogFiles = 2;

    auto editorLogger = createSharedConsoleLogger(editorOptions, consoleSink);
    m_namedLoggers["EDITOR"] = editorLogger;
}

std::shared_ptr<spdlog::logger> LoggerManager::createLogger(const LoggerOptions& options) {
    if (m_isShutdown) {
        return nullptr;
    }

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

std::shared_ptr<spdlog::logger> LoggerManager::createSharedConsoleLogger(
    const LoggerOptions& options,
    std::shared_ptr<spdlog::sinks::sink> sharedConsoleSink) {

    if (m_isShutdown) {
        return nullptr;
    }

    try {
        // Ensure log directory exists
        std::filesystem::create_directories(options.logDir);

        std::vector<spdlog::sink_ptr> sinks;

        // Use shared console sink if provided, otherwise create new one
        if (options.enableConsole) {
            if (sharedConsoleSink) {
                sinks.push_back(sharedConsoleSink);
            }
            else {
                auto console_sink = getSharedConsoleSink();
                console_sink->set_pattern(options.pattern);
                sinks.push_back(console_sink);
            }
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

std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> LoggerManager::getSharedConsoleSink() {
    if (m_isShutdown) {
        return nullptr;
    }

    if (!m_sharedConsoleSink) {
        m_sharedConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_sharedConsoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] - %v");
    }
    return m_sharedConsoleSink;
}

std::shared_ptr<spdlog::logger> LoggerManager::getNamedLogger(const std::string& name) {
    if (m_isShutdown) {
        return nullptr;
    }

    auto it = m_namedLoggers.find(name);
    if (it != m_namedLoggers.end()) {
        return it->second;
    }

    // If logger doesn't exist, create a basic one
    LoggerOptions options;
    options.loggerName = name;
    options.filePrefix = name + "_";
    options.logDir = "logs/";

    auto logger = createSharedConsoleLogger(options, getSharedConsoleSink());
    if (logger) {
        m_namedLoggers[name] = logger;
    }
    return logger;
}

bool LoggerManager::hasLogger(const std::string& name) const {
    return m_namedLoggers.find(name) != m_namedLoggers.end();
}

void LoggerManager::shutdown() {
    if (m_isShutdown) {
        return;
    }

    m_isShutdown = true;

    // Flush all loggers
    for (auto& [name, logger] : m_namedLoggers) {
        if (logger) {
            logger->flush();
        }
    }

    // Clear all loggers
    m_namedLoggers.clear();
    m_sharedConsoleSink.reset();

    // Shutdown spdlog
    spdlog::shutdown();
}

std::string LoggerManager::generateLogFilename(const LoggerOptions& options) {
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

void LoggerManager::rotateLogFiles(const LoggerOptions& options) {
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

// LoggerAccess implementation
void LoggerAccess::setLoggerManager(std::shared_ptr<LoggerManager> manager) {
    s_loggerManager = manager;
}

std::shared_ptr<LoggerManager> LoggerAccess::getLoggerManager() {
    return s_loggerManager.lock();
}

std::shared_ptr<spdlog::logger> LoggerAccess::getNamedLogger(const std::string& name) {
    auto manager = s_loggerManager.lock();
    if (manager) {
        return manager->getNamedLogger(name);
    }
    return nullptr;
}