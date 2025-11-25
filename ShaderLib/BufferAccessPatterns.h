#pragma once
#include <cstdint>
#include <string>
#include <json.hpp>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // ACCESS FREQUENCY - Jak często następują dostępy
    // ============================================================================
    enum class AccessFrequency : uint8_t {
        Never,              // Brak dostępu (np. GPU nie czyta z uniform buffer)
        OneTime,            // Jednorazowy dostęp (inicjalizacja)
        EveryFewFrames,     // Co kilka klatek (np. aktualizacja co 5-10 klatek)
        OncePerFrame,       // Raz na klatkę (standardowe uniform buffery)
        MultiplePerFrame,   // Kilka razy na klatkę (np. per-draw data)
        Continuous          // Ciągły dostęp (streaming, compute buffers)
    };

    // ============================================================================
    // ACCESS OPERATION - Typ operacji
    // ============================================================================
    enum class AccessOperation : uint8_t {
        None,       // Brak dostępu
        ReadOnly,   // Tylko odczyt
        WriteOnly,  // Tylko zapis
        ReadWrite   // Odczyt i zapis
    };

    // ============================================================================
    // ACCESS SIZE - Rozmiar typowego dostępu
    // ============================================================================
    enum class AccessSize : uint8_t {
        None,       // Brak dostępu
        Tiny,       // < 256 bytes (pojedyncze wartości, małe struktury)
        Small,      // 256B - 4KB (typowe uniform buffery)
        Medium,     // 4KB - 64KB (większe uniform buffery, małe storage)
        Large,      // 64KB - 1MB (duże storage buffery)
        VeryLarge,  // 1MB - 16MB (bardzo duże buffery, streaming)
        Massive     // > 16MB (ogromne bufory danych, compute)
    };

    // ============================================================================
    // PROCESSOR ACCESS PROFILE - Profil dostępu dla jednego procesora
    // ============================================================================
    struct ProcessorAccessProfile {
        AccessFrequency frequency;
        AccessOperation operation;
        AccessSize size;

        ProcessorAccessProfile()
            : frequency(AccessFrequency::Never)
            , operation(AccessOperation::None)
            , size(AccessSize::None)
        {
        }

        ProcessorAccessProfile(
            AccessFrequency freq,
            AccessOperation op,
            AccessSize sz
        )
            : frequency(freq)
            , operation(op)
            , size(sz)
        {
        }

        // Helpers
        bool HasAccess() const {
            return frequency != AccessFrequency::Never &&
                operation != AccessOperation::None;
        }

        bool IsReadOnly() const {
            return operation == AccessOperation::ReadOnly;
        }

        bool IsWriteOnly() const {
            return operation == AccessOperation::WriteOnly;
        }

        bool IsReadWrite() const {
            return operation == AccessOperation::ReadWrite;
        }

        bool CanRead() const {
            return operation == AccessOperation::ReadOnly ||
                operation == AccessOperation::ReadWrite;
        }

        bool CanWrite() const {
            return operation == AccessOperation::WriteOnly ||
                operation == AccessOperation::ReadWrite;
        }

        // Serializacja
        json ToJson() const;
        static ProcessorAccessProfile FromJson(const json& j);
    };

    // ============================================================================
    // BUFFER ACCESS PATTERNS - Kompletny profil dostępu do bufora
    // ============================================================================
    struct BufferAccessPatterns {
        ProcessorAccessProfile cpuAccess;
        ProcessorAccessProfile gpuAccess;

        BufferAccessPatterns() = default;

        BufferAccessPatterns(
            const ProcessorAccessProfile& cpu,
            const ProcessorAccessProfile& gpu
        )
            : cpuAccess(cpu)
            , gpuAccess(gpu)
        {
        }

        // Validation helpers
        bool IsValid() const;
        std::string GetValidationError() const;

        // Query helpers
        bool IsCPUVisible() const { return cpuAccess.HasAccess(); }
        bool IsGPUVisible() const { return gpuAccess.HasAccess(); }
        bool IsCPUReadable() const { return cpuAccess.CanRead(); }
        bool IsCPUWritable() const { return cpuAccess.CanWrite(); }
        bool IsGPUReadable() const { return gpuAccess.CanRead(); }
        bool IsGPUWritable() const { return gpuAccess.CanWrite(); }

        // Predefined patterns
        static BufferAccessPatterns UniformBuffer();        // CPU write once/frame, GPU read
        static BufferAccessPatterns StorageBuffer();        // CPU rare, GPU read/write

        // Serializacja
        json ToJson() const;
        static BufferAccessPatterns FromJson(const json& j);
    };

    // ============================================================================
    // STRING CONVERSIONS
    // ============================================================================
    const char* AccessFrequencyToString(AccessFrequency freq);
    const char* AccessOperationToString(AccessOperation op);
    const char* AccessSizeToString(AccessSize size);

    AccessFrequency StringToAccessFrequency(const std::string& str);
    AccessOperation StringToAccessOperation(const std::string& str);
    AccessSize StringToAccessSize(const std::string& str);

} // namespace ShaderLib
