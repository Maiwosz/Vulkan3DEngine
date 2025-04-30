#pragma once
#include <map>
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include "Event.h"

class Settings {
public:
    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off
    };

    enum class WindowMode {
        Windowed,
        Fullscreen,
        Borderless
    };

    enum class Resolution {
        R_640x480,
        R_800x600,
        R_1024x768,
        R_1280x720,
        R_1366x768,
        R_1600x900,
        R_1920x1080
    };

    enum class TextureFiltering {
        None,
        Bilinear,
        Trilinear,
        Anisotropic
    };

    enum class MsaaSampleCount {
        Samples1 = 1,
        Samples2 = 2,
        Samples4 = 4,
        Samples8 = 8,
        Samples16 = 16,
        Samples32 = 32,
        Samples64 = 64
    };

    struct ResolutionDetails {
        int width;
        int height;
        std::string name;
    };

    Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    // Wartości domyślne
    static constexpr WindowMode DEFAULT_WINDOW_MODE = WindowMode::Windowed;
    static constexpr Resolution DEFAULT_RESOLUTION = Resolution::R_1280x720;
    static constexpr bool DEFAULT_VSYNC_ENABLED = true;
    static constexpr TextureFiltering DEFAULT_TEXTURE_FILTER_MODE = TextureFiltering::Anisotropic;
    static constexpr MsaaSampleCount DEFAULT_MSAA_SAMPLES = MsaaSampleCount::Samples4;
    static constexpr uint32_t DEFAULT_FRAMES_IN_FLIGHT = 3;

    // Mapowania (pozostały static)
    static constexpr LogLevel DEFAULT_LOG_LEVEL = LogLevel::Info;
    static const std::map<LogLevel, std::string> LOG_LEVEL_MAP;
    static const std::map<Resolution, ResolutionDetails> RESOLUTION_MAP;
    static const std::map<MsaaSampleCount, uint32_t> MSAA_SAMPLE_MAP;
    static const std::map<WindowMode, std::string> WINDOW_MODE_MAP;
    static const std::map<TextureFiltering, std::string> TEXTURE_FILTERING_MAP;

    // Gettery i settery
    LogLevel getLogLevel() const { return m_logLevel; }
    WindowMode getWindowMode() const { return m_windowMode; }
    Resolution getResolution() const { return m_resolution; }
    bool isVsyncEnabled() const { return m_vsyncEnabled; }
    TextureFiltering getTextureFiltering() const { return m_textureFiltering; }
    MsaaSampleCount getMsaaSamples() const { return m_msaaSamples; }
    uint32_t getFramesInFlight() const { return m_framesInFlight; }

    MsaaSampleCount getMaxMsaaSamples() const { return m_maxMsaaSamples; }
    float getMaxAnisotropy() const { return m_maxAnisotropy; }
    bool isAnisotropySupported() const { return m_anisotropySupported; }

    void setLogLevel(LogLevel level);
    void setWindowMode(WindowMode mode);
    void setResolution(Resolution resolution);
    void setVsyncEnabled(bool enabled);
    void setTextureFiltering(TextureFiltering filtering);
    void setMsaaSamples(MsaaSampleCount samples);
    void setFramesInFlight(uint32_t count);

    ResolutionDetails getCurrentResolutionDetails() const;
    uint32_t getCurrentMsaaSampleCount() const;

    void setHardwareLimits(MsaaSampleCount maxMsaa, float maxAnisotropy, bool anisotropySupported);

    // Eventy
    std::shared_ptr<Event<LogLevel>> onLogLevelChanged() { return m_logLevelChangedEvent; }
    std::shared_ptr<Event<WindowMode>> onWindowModeChanged() { return m_windowModeChangedEvent; }
    std::shared_ptr<Event<Resolution>> onResolutionChanged() { return m_resolutionChangedEvent; }
    std::shared_ptr<Event<bool>> onVsyncChanged() { return m_vsyncChangedEvent; }
    std::shared_ptr<Event<TextureFiltering>> onTextureFilteringChanged() { return m_textureFilteringChangedEvent; }
    std::shared_ptr<Event<MsaaSampleCount>> onMsaaChanged() { return m_msaaChangedEvent; }
    std::shared_ptr<Event<uint32_t>> onFramesInFlightChanged() { return m_framesInFlightChangedEvent; }

    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);

    static spdlog::level::level_enum convertLogLevel(Settings::LogLevel level);

private:
    void initializeEvents();

    LogLevel m_logLevel;
    WindowMode m_windowMode;
    Resolution m_resolution;
    bool m_vsyncEnabled;
    TextureFiltering m_textureFiltering;
    MsaaSampleCount m_msaaSamples;
    uint32_t m_framesInFlight;

    bool m_hardwareLimitsSet = false;

    MsaaSampleCount m_maxMsaaSamples;
    float m_maxAnisotropy;
    bool m_anisotropySupported;

    std::shared_ptr<Event<LogLevel>> m_logLevelChangedEvent;
    std::shared_ptr<Event<WindowMode>> m_windowModeChangedEvent;
    std::shared_ptr<Event<Resolution>> m_resolutionChangedEvent;
    std::shared_ptr<Event<bool>> m_vsyncChangedEvent;
    std::shared_ptr<Event<TextureFiltering>> m_textureFilteringChangedEvent;
    std::shared_ptr<Event<MsaaSampleCount>> m_msaaChangedEvent;
    std::shared_ptr<Event<uint32_t>> m_framesInFlightChangedEvent;
};