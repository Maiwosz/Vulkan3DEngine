#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <variant>
#include <spdlog/spdlog.h>
#include "Event.h"

class Settings {
public:
    // Forward declarations for cleaner interface
    enum class LogLevel;
    enum class WindowMode;
    enum class Resolution;
    enum class TextureFiltering;
    enum class MipmapMode;
    enum class AnisotropyLevel;
    enum class MsaaSampleCount;

    enum class SettingType {
        LogLevel,
        WindowMode,
        Resolution,
        VSync,
        TextureFiltering,
        MipmapMode,
        AnisotropyLevel,
        MsaaSamples,
        FramesInFlight
    };

    // Variant type for all possible setting values
    using SettingValue = std::variant<
        LogLevel,
        WindowMode,
        Resolution,
        bool,
        TextureFiltering,
        MipmapMode,
        AnisotropyLevel,
        MsaaSampleCount,
        uint32_t
    >;

    // Event types for different setting changes - now type-safe
    using SettingChangedEvent = Event<SettingType, SettingValue, SettingValue>; // type, old_value, new_value
    using GraphicsSettingChangedEvent = Event<>;
    using DisplaySettingChangedEvent = Event<>;

    // Value types for type safety
    struct ResolutionInfo {
        int width, height;
        std::string displayName;

        bool operator==(const ResolutionInfo& other) const {
            return width == other.width && height == other.height;
        }
    };

    struct HardwareLimits {
        MsaaSampleCount maxMsaa;
        float maxAnisotropy;
        bool anisotropySupported;
    };

    struct SettingsBundle {
        LogLevel logLevel;
        WindowMode windowMode;
        Resolution resolution;
        bool vsyncEnabled;
        TextureFiltering textureFiltering;
        MipmapMode mipmapMode;
        AnisotropyLevel anisotropyLevel;
        MsaaSampleCount msaaSamples;
        uint32_t framesInFlight;
    };

public:
    Settings();
    ~Settings() = default;

    // Non-copyable but movable
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings(Settings&&) = default;
    Settings& operator=(Settings&&) = default;

    // === Core Interface ===

    // Logging
    LogLevel getLogLevel() const { return m_logLevel; }
    void setLogLevel(LogLevel level);

    // Display settings
    WindowMode getWindowMode() const { return m_windowMode; }
    void setWindowMode(WindowMode mode);

    Resolution getResolution() const { return m_resolution; }
    void setResolution(Resolution resolution);
    ResolutionInfo getResolutionInfo() const;

    bool isVsyncEnabled() const { return m_vsyncEnabled; }
    void setVsyncEnabled(bool enabled);

    // Graphics settings
    TextureFiltering getTextureFiltering() const { return m_textureFiltering; }
    void setTextureFiltering(TextureFiltering filtering);

    MipmapMode getMipmapMode() const { return m_mipmapMode; }
    void setMipmapMode(MipmapMode mode);

    AnisotropyLevel getAnisotropyLevel() const { return m_anisotropyLevel; }
    void setAnisotropyLevel(AnisotropyLevel level);
    float getCurrentAnisotropyValue() const;

    MsaaSampleCount getMsaaSamples() const { return m_msaaSamples; }
    void setMsaaSamples(MsaaSampleCount samples);
    uint32_t getCurrentMsaaSampleValue() const;

    uint32_t getFramesInFlight() const { return m_framesInFlight; }
    void setFramesInFlight(uint32_t count);

    // Hardware limits
    void setHardwareLimits(const HardwareLimits& limits);
    const HardwareLimits& getHardwareLimits() const { return m_hardwareLimits; }
    bool hasHardwareLimits() const { return m_hardwareLimitsSet; }

    // Batch operations
    SettingsBundle getAllSettings() const;
    void applySettings(const SettingsBundle& bundle);
    static SettingsBundle getDefaultSettings();

    // Persistence
    bool save(const std::string& filename = "settings.json") const;
    bool load(const std::string& filename = "settings.json");

    // Events - subscribe to setting changes
    [[nodiscard]] auto onSettingChanged() -> SettingChangedEvent& {
        return m_settingChangedEvent;
    }
    [[nodiscard]] auto onGraphicsSettingChanged() -> GraphicsSettingChangedEvent& {
        return m_graphicsSettingChangedEvent;
    }
    [[nodiscard]] auto onDisplaySettingChanged() -> DisplaySettingChangedEvent& {
        return m_displaySettingChangedEvent;
    }

    // Utility functions
    static std::string toString(SettingType type);
    static std::string toString(const SettingValue& value);
    static spdlog::level::level_enum toSpdlogLevel(LogLevel level);
    static std::string toString(LogLevel level);
    static std::string toString(WindowMode mode);
    static std::string toString(TextureFiltering filtering);
    static std::string toString(MipmapMode mode);
    static std::string toString(Resolution resolution);
    static std::string toString(AnisotropyLevel level);
    static std::string toString(MsaaSampleCount samples);

private:
    // Helper methods
    template<typename T>
    void updateSetting(T& current, T newValue, SettingType settingType);

    void updateGraphicsSetting(auto& current, auto newValue, SettingType settingType);
    void updateDisplaySetting(auto& current, auto newValue, SettingType settingType);

    bool validateAndClampMsaa(MsaaSampleCount& samples) const;
    bool validateAndClampAnisotropy(AnisotropyLevel& level) const;
    bool validateTextureFiltering(TextureFiltering& filtering) const;

    // Helper to create SettingValue from any type
    template<typename T>
    static SettingValue makeSettingValue(const T& value);

private:
    // Settings data
    LogLevel m_logLevel;
    WindowMode m_windowMode;
    Resolution m_resolution;
    bool m_vsyncEnabled;
    TextureFiltering m_textureFiltering;
    MipmapMode m_mipmapMode;
    AnisotropyLevel m_anisotropyLevel;
    MsaaSampleCount m_msaaSamples;
    uint32_t m_framesInFlight;

    // Hardware constraints
    HardwareLimits m_hardwareLimits;
    bool m_hardwareLimitsSet = false;

    // Events
    SettingChangedEvent m_settingChangedEvent;
    GraphicsSettingChangedEvent m_graphicsSettingChangedEvent;
    DisplaySettingChangedEvent m_displaySettingChangedEvent;
};

// === Enum Definitions ===

enum class Settings::LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

enum class Settings::WindowMode : int {
    Windowed = 0,
    Fullscreen = 1,
    Borderless = 2
};

enum class Settings::Resolution : int {
    R_640x480 = 0,
    R_800x600 = 1,
    R_1024x768 = 2,
    R_1280x720 = 3,
    R_1366x768 = 4,
    R_1600x900 = 5,
    R_1920x1080 = 6
};

enum class Settings::TextureFiltering : int {
    None = 0,
    Bilinear = 1,
    Trilinear = 2,
    Anisotropic = 3
};

enum class Settings::MipmapMode : int {
    Nearest = 0,
    Linear = 1
};

enum class Settings::AnisotropyLevel : int {
    X1 = 1,
    X2 = 2,
    X4 = 4,
    X8 = 8,
    X16 = 16
};

enum class Settings::MsaaSampleCount : int {
    Samples1 = 1,
    Samples2 = 2,
    Samples4 = 4,
    Samples8 = 8,
    Samples16 = 16,
    Samples32 = 32,
    Samples64 = 64
};

// === Default Values ===
namespace SettingsDefaults {
    constexpr Settings::LogLevel LOG_LEVEL = Settings::LogLevel::Info;
    constexpr Settings::WindowMode WINDOW_MODE = Settings::WindowMode::Windowed;
    constexpr Settings::Resolution RESOLUTION = Settings::Resolution::R_1280x720;
    constexpr bool VSYNC_ENABLED = true;
    constexpr Settings::TextureFiltering TEXTURE_FILTERING = Settings::TextureFiltering::Anisotropic;
    constexpr Settings::MipmapMode MIPMAP_MODE = Settings::MipmapMode::Linear;
    constexpr Settings::AnisotropyLevel ANISOTROPY_LEVEL = Settings::AnisotropyLevel::X8;
    constexpr Settings::MsaaSampleCount MSAA_SAMPLES = Settings::MsaaSampleCount::Samples4;
    constexpr uint32_t FRAMES_IN_FLIGHT = 3;
}