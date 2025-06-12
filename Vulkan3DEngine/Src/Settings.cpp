#include "Settings.h"
#include <fstream>
#include <json.hpp>
#include <algorithm>

// === Static Data ===

namespace {
    // Lookup tables - encapsulated in anonymous namespace
    const std::unordered_map<Settings::SettingType, std::string> SETTING_TYPE_NAMES = {
        {Settings::SettingType::LogLevel, "LogLevel"},
        {Settings::SettingType::WindowMode, "WindowMode"},
        {Settings::SettingType::Resolution, "Resolution"},
        {Settings::SettingType::VSync, "VSync"},
        {Settings::SettingType::TextureFiltering, "TextureFiltering"},
        {Settings::SettingType::MipmapMode, "MipmapMode"},
        {Settings::SettingType::AnisotropyLevel, "AnisotropyLevel"},
        {Settings::SettingType::MsaaSamples, "MsaaSamples"},
        {Settings::SettingType::FramesInFlight, "FramesInFlight"}
    };

    const std::unordered_map<Settings::LogLevel, std::string> LOG_LEVEL_NAMES = {
        {Settings::LogLevel::Trace, "Trace"},
        {Settings::LogLevel::Debug, "Debug"},
        {Settings::LogLevel::Info, "Info"},
        {Settings::LogLevel::Warn, "Warn"},
        {Settings::LogLevel::Error, "Error"},
        {Settings::LogLevel::Critical, "Critical"},
        {Settings::LogLevel::Off, "Off"}
    };

    const std::unordered_map<Settings::WindowMode, std::string> WINDOW_MODE_NAMES = {
        {Settings::WindowMode::Windowed, "Windowed"},
        {Settings::WindowMode::Fullscreen, "Fullscreen"},
        {Settings::WindowMode::Borderless, "Borderless"}
    };

    const std::unordered_map<Settings::TextureFiltering, std::string> TEXTURE_FILTERING_NAMES = {
        {Settings::TextureFiltering::None, "None"},
        {Settings::TextureFiltering::Bilinear, "Bilinear"},
        {Settings::TextureFiltering::Trilinear, "Trilinear"},
        {Settings::TextureFiltering::Anisotropic, "Anisotropic"}
    };

    const std::unordered_map<Settings::MipmapMode, std::string> MIPMAP_MODE_NAMES = {
        {Settings::MipmapMode::Nearest, "Nearest"},
        {Settings::MipmapMode::Linear, "Linear"}
    };

    const std::unordered_map<Settings::Resolution, Settings::ResolutionInfo> RESOLUTION_INFO = {
        {Settings::Resolution::R_640x480, {640, 480, "640x480"}},
        {Settings::Resolution::R_800x600, {800, 600, "800x600"}},
        {Settings::Resolution::R_1024x768, {1024, 768, "1024x768"}},
        {Settings::Resolution::R_1280x720, {1280, 720, "1280x720"}},
        {Settings::Resolution::R_1366x768, {1366, 768, "1366x768"}},
        {Settings::Resolution::R_1600x900, {1600, 900, "1600x900"}},
        {Settings::Resolution::R_1920x1080, {1920, 1080, "1920x1080"}}
    };

    constexpr uint32_t MIN_FRAMES = 1;
    constexpr uint32_t MAX_FRAMES = 3;
}

// === Constructor ===

Settings::Settings()
    : m_logLevel(SettingsDefaults::LOG_LEVEL)
    , m_windowMode(SettingsDefaults::WINDOW_MODE)
    , m_resolution(SettingsDefaults::RESOLUTION)
    , m_vsyncEnabled(SettingsDefaults::VSYNC_ENABLED)
    , m_textureFiltering(SettingsDefaults::TEXTURE_FILTERING)
    , m_mipmapMode(SettingsDefaults::MIPMAP_MODE)
    , m_anisotropyLevel(SettingsDefaults::ANISOTROPY_LEVEL)
    , m_msaaSamples(SettingsDefaults::MSAA_SAMPLES)
    , m_framesInFlight(SettingsDefaults::FRAMES_IN_FLIGHT)
    , m_hardwareLimits{ MsaaSampleCount::Samples1, 1.0f, false }
{
}

// === Public Interface ===

void Settings::setLogLevel(LogLevel level) {
    updateSetting(m_logLevel, level, SettingType::LogLevel);
}

void Settings::setWindowMode(WindowMode mode) {
    updateDisplaySetting(m_windowMode, mode, SettingType::WindowMode);
}

void Settings::setResolution(Resolution resolution) {
    updateDisplaySetting(m_resolution, resolution, SettingType::Resolution);
}

void Settings::setVsyncEnabled(bool enabled) {
    updateDisplaySetting(m_vsyncEnabled, enabled, SettingType::VSync);
}

void Settings::setTextureFiltering(TextureFiltering filtering) {
    auto originalFiltering = filtering;

    if (!validateTextureFiltering(filtering)) {
        SPDLOG_WARN("Texture filtering {} not supported, using {}",
            toString(originalFiltering), toString(filtering));
    }

    updateGraphicsSetting(m_textureFiltering, filtering, SettingType::TextureFiltering);
}

void Settings::setMipmapMode(MipmapMode mode) {
    updateGraphicsSetting(m_mipmapMode, mode, SettingType::MipmapMode);
}

void Settings::setAnisotropyLevel(AnisotropyLevel level) {
    auto originalLevel = level;

    if (!validateAndClampAnisotropy(level)) {
        SPDLOG_WARN("Anisotropy level {}x exceeds maximum {}x, clamped to {}x",
            static_cast<int>(originalLevel),
            m_hardwareLimits.maxAnisotropy,
            static_cast<int>(level));
    }

    updateGraphicsSetting(m_anisotropyLevel, level, SettingType::AnisotropyLevel);
}

void Settings::setMsaaSamples(MsaaSampleCount samples) {
    auto originalSamples = samples;

    if (!validateAndClampMsaa(samples)) {
        SPDLOG_WARN("MSAA samples {}x exceeds maximum {}x, clamped to {}x",
            static_cast<int>(originalSamples),
            static_cast<int>(m_hardwareLimits.maxMsaa),
            static_cast<int>(samples));
    }

    updateGraphicsSetting(m_msaaSamples, samples, SettingType::MsaaSamples);
}

void Settings::setFramesInFlight(uint32_t count) {
    auto originalCount = count;
    count = std::clamp(count, MIN_FRAMES, MAX_FRAMES);

    if (originalCount != count) {
        SPDLOG_WARN("Frames in flight clamped from {} to {} (valid range: {}-{})",
            originalCount, count, MIN_FRAMES, MAX_FRAMES);
    }

    updateGraphicsSetting(m_framesInFlight, count, SettingType::FramesInFlight);
}

void Settings::setHardwareLimits(const HardwareLimits& limits) {
    m_hardwareLimits = limits;
    m_hardwareLimitsSet = true;

    SPDLOG_INFO("Hardware limits set - Max MSAA: {}x, Max Anisotropy: {}x, Anisotropy supported: {}",
        static_cast<int>(limits.maxMsaa),
        limits.maxAnisotropy,
        limits.anisotropySupported ? "Yes" : "No");

    // Revalidate current settings against new limits
    auto currentMsaa = m_msaaSamples;
    auto currentAnisotropy = m_anisotropyLevel;
    auto currentFiltering = m_textureFiltering;

    setMsaaSamples(currentMsaa);
    setAnisotropyLevel(currentAnisotropy);
    setTextureFiltering(currentFiltering);
}

Settings::ResolutionInfo Settings::getResolutionInfo() const {
    auto it = RESOLUTION_INFO.find(m_resolution);
    return it != RESOLUTION_INFO.end() ? it->second : ResolutionInfo{ 1280, 720, "1280x720" };
}

float Settings::getCurrentAnisotropyValue() const {
    if (m_textureFiltering == TextureFiltering::Anisotropic && m_hardwareLimits.anisotropySupported) {
        return static_cast<float>(m_anisotropyLevel);
    }
    return 1.0f;
}

uint32_t Settings::getCurrentMsaaSampleValue() const {
    return static_cast<uint32_t>(m_msaaSamples);
}

// === Persistence ===

bool Settings::save(const std::string& filename) const {
    try {
        nlohmann::json json;

        json["logLevel"] = static_cast<int>(m_logLevel);
        json["windowMode"] = static_cast<int>(m_windowMode);
        json["resolution"] = static_cast<int>(m_resolution);
        json["vsyncEnabled"] = m_vsyncEnabled;
        json["textureFiltering"] = static_cast<int>(m_textureFiltering);
        json["mipmapMode"] = static_cast<int>(m_mipmapMode);
        json["anisotropyLevel"] = static_cast<int>(m_anisotropyLevel);
        json["msaaSamples"] = static_cast<int>(m_msaaSamples);
        json["framesInFlight"] = m_framesInFlight;

        std::ofstream file(filename);
        if (!file.is_open()) {
            SPDLOG_ERROR("Failed to open settings file for writing: {}", filename);
            return false;
        }

        file << std::setw(4) << json << std::endl;
        SPDLOG_INFO("Settings saved to {}", filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save settings to {}: {}", filename, e.what());
        return false;
    }
}

bool Settings::load(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            SPDLOG_WARN("Settings file not found: {}, using defaults", filename);
            return false;
        }

        nlohmann::json json;
        file >> json;

        SPDLOG_INFO("Loading settings from {}", filename);

        // Load basic settings
        if (json.contains("logLevel")) {
            m_logLevel = static_cast<LogLevel>(json["logLevel"].get<int>());
        }

        if (json.contains("windowMode")) {
            m_windowMode = static_cast<WindowMode>(json["windowMode"].get<int>());
        }

        if (json.contains("resolution")) {
            m_resolution = static_cast<Resolution>(json["resolution"].get<int>());
        }

        if (json.contains("vsyncEnabled")) {
            m_vsyncEnabled = json["vsyncEnabled"].get<bool>();
        }

        if (json.contains("textureFiltering")) {
            m_textureFiltering = static_cast<TextureFiltering>(json["textureFiltering"].get<int>());
        }

        if (json.contains("mipmapMode")) {
            m_mipmapMode = static_cast<MipmapMode>(json["mipmapMode"].get<int>());
        }

        if (json.contains("anisotropyLevel")) {
            m_anisotropyLevel = static_cast<AnisotropyLevel>(json["anisotropyLevel"].get<int>());
        }

        if (json.contains("msaaSamples")) {
            m_msaaSamples = static_cast<MsaaSampleCount>(json["msaaSamples"].get<int>());
        }

        if (json.contains("framesInFlight")) {
            m_framesInFlight = json["framesInFlight"].get<uint32_t>();
        }

        // Revalidate settings if hardware limits are set
        if (m_hardwareLimitsSet) {
            auto msaa = m_msaaSamples;
            auto anisotropy = m_anisotropyLevel;
            auto filtering = m_textureFiltering;

            validateAndClampMsaa(msaa);
            validateAndClampAnisotropy(anisotropy);
            validateTextureFiltering(filtering);

            m_msaaSamples = msaa;
            m_anisotropyLevel = anisotropy;
            m_textureFiltering = filtering;
        }

        SPDLOG_INFO("Settings loaded successfully from {}", filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load settings from {}: {}", filename, e.what());
        return false;
    }
}

// === Static Utility Functions ===

std::string Settings::toString(SettingType type) {
    auto it = SETTING_TYPE_NAMES.find(type);
    return it != SETTING_TYPE_NAMES.end() ? it->second : "Unknown";
}

std::string Settings::toString(const SettingValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, bool>) {
            return v ? "enabled" : "disabled";
        }
        else if constexpr (std::is_same_v<T, LogLevel>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, WindowMode>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, Resolution>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, TextureFiltering>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, MipmapMode>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, AnisotropyLevel>) {
            return toString(v);
        }
        else if constexpr (std::is_same_v<T, MsaaSampleCount>) {
            return toString(v);
        }
        else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(v);
        }
        else {
            return "Unknown";
        }
        }, value);
}

spdlog::level::level_enum Settings::toSpdlogLevel(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return spdlog::level::trace;
    case LogLevel::Debug: return spdlog::level::debug;
    case LogLevel::Info: return spdlog::level::info;
    case LogLevel::Warn: return spdlog::level::warn;
    case LogLevel::Error: return spdlog::level::err;
    case LogLevel::Critical: return spdlog::level::critical;
    case LogLevel::Off: return spdlog::level::off;
    default: return spdlog::level::info;
    }
}

std::string Settings::toString(LogLevel level) {
    auto it = LOG_LEVEL_NAMES.find(level);
    return it != LOG_LEVEL_NAMES.end() ? it->second : "Unknown";
}

std::string Settings::toString(WindowMode mode) {
    auto it = WINDOW_MODE_NAMES.find(mode);
    return it != WINDOW_MODE_NAMES.end() ? it->second : "Unknown";
}

std::string Settings::toString(TextureFiltering filtering) {
    auto it = TEXTURE_FILTERING_NAMES.find(filtering);
    return it != TEXTURE_FILTERING_NAMES.end() ? it->second : "Unknown";
}

std::string Settings::toString(MipmapMode mode) {
    auto it = MIPMAP_MODE_NAMES.find(mode);
    return it != MIPMAP_MODE_NAMES.end() ? it->second : "Unknown";
}

std::string Settings::toString(Resolution resolution) {
    auto it = RESOLUTION_INFO.find(resolution);
    return it != RESOLUTION_INFO.end() ? it->second.displayName : "Unknown";
}

std::string Settings::toString(AnisotropyLevel level) {
    return std::to_string(static_cast<int>(level)) + "x";
}

std::string Settings::toString(MsaaSampleCount samples) {
    return std::to_string(static_cast<int>(samples)) + "x";
}

// === Private Helper Methods ===

template<typename T>
Settings::SettingValue Settings::makeSettingValue(const T& value) {
    return SettingValue{ value };
}

template<typename T>
void Settings::updateSetting(T& current, T newValue, SettingType settingType) {
    if (current != newValue) {
        auto oldValue = makeSettingValue(current);
        auto newValueVariant = makeSettingValue(newValue);

        SPDLOG_INFO("{} changed: {} → {}",
            toString(settingType),
            toString(oldValue),
            toString(newValueVariant));

        current = newValue;

        // Fire events with type-safe values
        m_settingChangedEvent.invoke(settingType, oldValue, newValueVariant);
    }
}

void Settings::updateGraphicsSetting(auto& current, auto newValue, SettingType settingType) {
    if (current != newValue) {
        updateSetting(current, newValue, settingType);
        m_graphicsSettingChangedEvent.invoke();
    }
}

void Settings::updateDisplaySetting(auto& current, auto newValue, SettingType settingType) {
    if (current != newValue) {
        updateSetting(current, newValue, settingType);
        m_displaySettingChangedEvent.invoke();
    }
}

bool Settings::validateAndClampMsaa(MsaaSampleCount& samples) const {
    if (!m_hardwareLimitsSet) return true;

    if (static_cast<int>(samples) > static_cast<int>(m_hardwareLimits.maxMsaa)) {
        samples = m_hardwareLimits.maxMsaa;
        return false;
    }
    return true;
}

bool Settings::validateAndClampAnisotropy(AnisotropyLevel& level) const {
    if (!m_hardwareLimitsSet || !m_hardwareLimits.anisotropySupported) return true;

    float levelValue = static_cast<float>(level);
    if (levelValue > m_hardwareLimits.maxAnisotropy) {
        // Find the highest supported level
        if (m_hardwareLimits.maxAnisotropy >= 16.0f) level = AnisotropyLevel::X16;
        else if (m_hardwareLimits.maxAnisotropy >= 8.0f) level = AnisotropyLevel::X8;
        else if (m_hardwareLimits.maxAnisotropy >= 4.0f) level = AnisotropyLevel::X4;
        else if (m_hardwareLimits.maxAnisotropy >= 2.0f) level = AnisotropyLevel::X2;
        else level = AnisotropyLevel::X1;
        return false;
    }
    return true;
}

bool Settings::validateTextureFiltering(TextureFiltering& filtering) const {
    if (!m_hardwareLimitsSet) return true;

    if (filtering == TextureFiltering::Anisotropic && !m_hardwareLimits.anisotropySupported) {
        filtering = TextureFiltering::Trilinear;
        return false;
    }
    return true;
}