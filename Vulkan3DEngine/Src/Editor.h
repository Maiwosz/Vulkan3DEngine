#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <spdlog/spdlog.h>
#include "AssetWatcher.h"
#include "EditorUI.h"
#include "Engine.h"

class LoggerManager; // Forward declaration

class Editor {
public:
    Editor(const std::string& sourceRelative, const std::string& destRelative);
    ~Editor();

    void start();
    void stop();

    AssetWatcher& assetWatcher() { return *m_assetWatcher; }
    EditorUI& ui() { return *m_editorUI; }
    Engine& engine() { return *m_engine; }

private:
    void initializeEngine();

    std::unique_ptr<Engine> m_engine;
    std::unique_ptr<AssetWatcher> m_assetWatcher;
    std::unique_ptr<EditorUI> m_editorUI;
    std::thread m_watcherThread;
    std::atomic<bool> m_running{ false };

    // Shared ownership of LoggerManager to ensure it survives
    std::shared_ptr<LoggerManager> m_loggerManager;

    // Engine initialization parameters
    std::string m_sourceRelative;
    std::string m_destRelative;
};