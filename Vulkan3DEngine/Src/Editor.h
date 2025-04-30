#pragma once
#include "AssetWatcher.h"
#include <thread>
#include <atomic>
#include <spdlog/logger.h>
#include <spdlog/details/registry.h>

class Editor {
public:
    Editor(const std::string& sourceRelative, const std::string& destRelative);
    ~Editor();

    void start();
    void stop();

    std::shared_ptr<spdlog::logger> getLogger() { return m_logger; }
private:
    std::unique_ptr<AssetWatcher> m_assetWatcher;
    std::thread m_watcherThread;
    std::atomic<bool> m_running{ false };
    std::shared_ptr<spdlog::logger> m_logger;

    void configureLogger();
};