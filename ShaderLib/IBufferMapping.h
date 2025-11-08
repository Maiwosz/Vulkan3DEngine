#pragma once
#include <cstddef>

namespace ShaderLib {

    // Interface for buffers that can be mapped to CPU-accessible memory
    // Designed for persistent mapping pattern with fallback support
    class IBufferMapping {
    public:
        virtual ~IBufferMapping() = default;

        // PRIMARY API: Direct data access (expects persistent mapping)
        // Returns pointer to mapped data or nullptr if not mapped
        virtual void* getMappedPointer() = 0;
        virtual const void* getMappedPointer() const = 0;

        // Query mapping state
        virtual bool isMapped() const = 0;

        // Get allocated size (for bounds checking)
        virtual size_t getAllocatedSize() const = 0;

        // FALLBACK API: Manual mapping control (only if persistent mapping failed)
        // These should rarely be needed in normal operation
        virtual void* map() = 0;      // Map buffer, returns pointer
        virtual void unmap() = 0;     // Unmap buffer
    };

} // namespace ShaderLib
