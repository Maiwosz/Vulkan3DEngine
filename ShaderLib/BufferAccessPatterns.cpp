#include "pch.h"
#include "BufferAccessPatterns.h"
#include <stdexcept>
#include <sstream>

namespace ShaderLib {

    // ============================================================================
    // PROCESSOR ACCESS PROFILE - SERIALIZATION
    // ============================================================================

    json ProcessorAccessProfile::ToJson() const {
        json j;
        j["frequency"] = static_cast<int>(frequency);
        j["operation"] = static_cast<int>(operation);
        j["size"] = static_cast<int>(size);
        return j;
    }

    ProcessorAccessProfile ProcessorAccessProfile::FromJson(const json& j) {
        ProcessorAccessProfile profile;
        profile.frequency = static_cast<AccessFrequency>(
            j.value("frequency", static_cast<int>(AccessFrequency::Never))
            );
        profile.operation = static_cast<AccessOperation>(
            j.value("operation", static_cast<int>(AccessOperation::None))
            );
        profile.size = static_cast<AccessSize>(
            j.value("size", static_cast<int>(AccessSize::None))
            );
        return profile;
    }

    // ============================================================================
    // BUFFER ACCESS PATTERNS - VALIDATION
    // ============================================================================

    bool BufferAccessPatterns::IsValid() const {
        // Jeśli procesor ma dostęp, musi mieć poprawne frequency i operation
        if (cpuAccess.HasAccess()) {
            if (cpuAccess.frequency == AccessFrequency::Never ||
                cpuAccess.operation == AccessOperation::None) {
                return false;
            }
        }

        if (gpuAccess.HasAccess()) {
            if (gpuAccess.frequency == AccessFrequency::Never ||
                gpuAccess.operation == AccessOperation::None) {
                return false;
            }
        }

        // Conajmniej jeden procesor musi mieć dostęp
        if (!cpuAccess.HasAccess() && !gpuAccess.HasAccess()) {
            return false;
        }

        return true;
    }

    std::string BufferAccessPatterns::GetValidationError() const {
        if (!IsValid()) {
            std::stringstream ss;

            if (!cpuAccess.HasAccess() && !gpuAccess.HasAccess()) {
                ss << "Neither CPU nor GPU has access to buffer";
            }
            else {
                if (cpuAccess.HasAccess()) {
                    if (cpuAccess.frequency == AccessFrequency::Never) {
                        ss << "CPU access has invalid frequency (Never); ";
                    }
                    if (cpuAccess.operation == AccessOperation::None) {
                        ss << "CPU access has invalid operation (None); ";
                    }
                }
                if (gpuAccess.HasAccess()) {
                    if (gpuAccess.frequency == AccessFrequency::Never) {
                        ss << "GPU access has invalid frequency (Never); ";
                    }
                    if (gpuAccess.operation == AccessOperation::None) {
                        ss << "GPU access has invalid operation (None); ";
                    }
                }
            }

            return ss.str();
        }

        return "Valid";
    }

    // ============================================================================
    // PREDEFINED PATTERNS
    // ============================================================================

    BufferAccessPatterns BufferAccessPatterns::UniformBuffer() {
        return BufferAccessPatterns(
            ProcessorAccessProfile(
                AccessFrequency::OncePerFrame,
                AccessOperation::WriteOnly,
                AccessSize::Small
            ),
            ProcessorAccessProfile(
                AccessFrequency::MultiplePerFrame,
                AccessOperation::ReadOnly,
                AccessSize::Small
            )
        );
    }

    BufferAccessPatterns BufferAccessPatterns::StorageBuffer() {
        return BufferAccessPatterns(
            ProcessorAccessProfile(
                AccessFrequency::EveryFewFrames,
                AccessOperation::ReadWrite,
                AccessSize::Medium
            ),
            ProcessorAccessProfile(
                AccessFrequency::OncePerFrame,
                AccessOperation::ReadWrite,
                AccessSize::Medium
            )
        );
    }

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    json BufferAccessPatterns::ToJson() const {
        json j;
        j["cpuAccess"] = cpuAccess.ToJson();
        j["gpuAccess"] = gpuAccess.ToJson();
        return j;
    }

    BufferAccessPatterns BufferAccessPatterns::FromJson(const json& j) {
        BufferAccessPatterns patterns;

        if (j.contains("cpuAccess")) {
            patterns.cpuAccess = ProcessorAccessProfile::FromJson(j.at("cpuAccess"));
        }

        if (j.contains("gpuAccess")) {
            patterns.gpuAccess = ProcessorAccessProfile::FromJson(j.at("gpuAccess"));
        }

        return patterns;
    }

    // ============================================================================
    // STRING CONVERSIONS
    // ============================================================================

    const char* AccessFrequencyToString(AccessFrequency freq) {
        switch (freq) {
        case AccessFrequency::Never: return "Never";
        case AccessFrequency::OneTime: return "OneTime";
        case AccessFrequency::EveryFewFrames: return "EveryFewFrames";
        case AccessFrequency::OncePerFrame: return "OncePerFrame";
        case AccessFrequency::MultiplePerFrame: return "MultiplePerFrame";
        case AccessFrequency::Continuous: return "Continuous";
        default: return "Unknown";
        }
    }

    const char* AccessOperationToString(AccessOperation op) {
        switch (op) {
        case AccessOperation::None: return "None";
        case AccessOperation::ReadOnly: return "ReadOnly";
        case AccessOperation::WriteOnly: return "WriteOnly";
        case AccessOperation::ReadWrite: return "ReadWrite";
        default: return "Unknown";
        }
    }

    const char* AccessSizeToString(AccessSize size) {
        switch (size) {
        case AccessSize::None: return "None";
        case AccessSize::Tiny: return "Tiny (<256B)";
        case AccessSize::Small: return "Small (256B-4KB)";
        case AccessSize::Medium: return "Medium (4KB-64KB)";
        case AccessSize::Large: return "Large (64KB-1MB)";
        case AccessSize::VeryLarge: return "VeryLarge (1MB-16MB)";
        case AccessSize::Massive: return "Massive (>16MB)";
        default: return "Unknown";
        }
    }

    AccessFrequency StringToAccessFrequency(const std::string& str) {
        if (str == "Never") return AccessFrequency::Never;
        if (str == "OneTime") return AccessFrequency::OneTime;
        if (str == "EveryFewFrames") return AccessFrequency::EveryFewFrames;
        if (str == "OncePerFrame") return AccessFrequency::OncePerFrame;
        if (str == "MultiplePerFrame") return AccessFrequency::MultiplePerFrame;
        if (str == "Continuous") return AccessFrequency::Continuous;
        throw std::runtime_error("Unknown AccessFrequency: " + str);
    }

    AccessOperation StringToAccessOperation(const std::string& str) {
        if (str == "None") return AccessOperation::None;
        if (str == "ReadOnly") return AccessOperation::ReadOnly;
        if (str == "WriteOnly") return AccessOperation::WriteOnly;
        if (str == "ReadWrite") return AccessOperation::ReadWrite;
        throw std::runtime_error("Unknown AccessOperation: " + str);
    }

    AccessSize StringToAccessSize(const std::string& str) {
        if (str == "None") return AccessSize::None;
        if (str == "Tiny") return AccessSize::Tiny;
        if (str == "Small") return AccessSize::Small;
        if (str == "Medium") return AccessSize::Medium;
        if (str == "Large") return AccessSize::Large;
        if (str == "VeryLarge") return AccessSize::VeryLarge;
        if (str == "Massive") return AccessSize::Massive;
        throw std::runtime_error("Unknown AccessSize: " + str);
    }

} // namespace ShaderLib
