#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <spdlog/spdlog.h>
#include "AssetWatcher.h"

class Editor {
public:
    Editor(const std::string& sourceRelative, const std::string& destRelative);
    ~Editor();

    void start();
    void stop();

    AssetWatcher& assetWatcher() { return *m_assetWatcher; }

    std::shared_ptr<spdlog::logger> getLogger() const { return m_logger; }

private:
    void configureLogger();

    std::unique_ptr<AssetWatcher> m_assetWatcher;
    std::thread m_watcherThread;
    std::atomic<bool> m_running{ false };
    std::shared_ptr<spdlog::logger> m_logger;
};