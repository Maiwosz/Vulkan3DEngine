#pragma once
#include <stdexcept>
#include <string>

// ============================================================================
// SHADER LIB VALIDATION SYSTEM
// ============================================================================
// Trzy poziomy walidacji:
// 1. SHADER_ASSERT    - tylko w debug, zero overhead w release
// 2. SHADER_VERIFY    - w debug rzuca wyjątek, w release tylko assert
// 3. SHADER_VALIDATE  - zawsze aktywne, dla krytycznych sprawdzeń
// ============================================================================

namespace ShaderLib {

    // Helper dla formatowania błędów
    template<typename... Args>
    std::string FormatError(const char* fmt, Args&&... args) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
        return std::string(buffer);
    }

} // namespace ShaderLib

// ============================================================================
// DEBUG MODE - Pełna walidacja
// ============================================================================
#if defined(_DEBUG)
#include <cassert>

// Assert bez wyjątku - tylko w debug
#define SHADER_ASSERT(condition, message) \
        assert((condition) && (message))

    // Assert z wyjątkiem - tylko w debug
#define SHADER_VERIFY(condition, message) \
        do { \
            if (!(condition)) { \
                throw std::runtime_error(message); \
            } \
        } while(0)

    // Verify z formatowaniem
#define SHADER_VERIFY_FMT(condition, fmt, ...) \
        do { \
            if (!(condition)) { \
                throw std::runtime_error( \
                    ShaderLib::FormatError(fmt, __VA_ARGS__) \
                ); \
            } \
        } while(0)

// ============================================================================
// RELEASE MODE - Minimalna walidacja
// ============================================================================
#else
    // W release asserty nic nie robią
#define SHADER_ASSERT(condition, message) ((void)0)
#define SHADER_VERIFY(condition, message) ((void)0)
#define SHADER_VERIFY_FMT(condition, fmt, ...) ((void)0)
#endif

// ============================================================================
// CRITICAL VALIDATION - Zawsze aktywne
// ============================================================================
// Używaj tylko dla rzeczy które MUSZĄ być sprawdzone w release:
// - nullptr checks na krytycznych wskaźnikach
// - buffer overflow protection
// - finalization checks

#define SHADER_VALIDATE(condition, message) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error(message); \
        } \
    } while(0)

#define SHADER_VALIDATE_FMT(condition, fmt, ...) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error( \
                ShaderLib::FormatError(fmt, __VA_ARGS__) \
            ); \
        } \
    } while(0)

// ============================================================================
// NULLPTR CHECKS
// ============================================================================
#define SHADER_CHECK_NULL(ptr, name) \
    SHADER_VALIDATE((ptr) != nullptr, name " cannot be null")

// ============================================================================
// FINALIZATION CHECKS
// ============================================================================
#define SHADER_CHECK_FINALIZED(obj) \
    SHADER_VALIDATE((obj)->IsFinalized(), \
        "Structure must be finalized before use")

#define SHADER_CHECK_NOT_FINALIZED(obj) \
    SHADER_VERIFY((obj)->IsFinalized() == false, \
        "Cannot modify finalized structure")
