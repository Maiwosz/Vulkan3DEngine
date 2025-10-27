#pragma once
#include "Buffer.h"
#include <utility>

// ============================================================================
// BASE CLASS - Common RAII behavior for mapped buffer access
// ============================================================================

template<typename AccessorType>
class MappedBufferAccessor {
public:
    MappedBufferAccessor() = default;

    // Move-only type (no copying mapped buffers)
    MappedBufferAccessor(const MappedBufferAccessor&) = delete;
    MappedBufferAccessor& operator=(const MappedBufferAccessor&) = delete;

    MappedBufferAccessor(MappedBufferAccessor&& other) noexcept
        : m_buffer(other.m_buffer)
        , m_accessor(std::move(other.m_accessor))
        , m_isMapped(other.m_isMapped) {
        other.m_buffer = nullptr;
        other.m_isMapped = false;
    }

    MappedBufferAccessor& operator=(MappedBufferAccessor&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_buffer = other.m_buffer;
            m_accessor = std::move(other.m_accessor);
            m_isMapped = other.m_isMapped;
            other.m_buffer = nullptr;
            other.m_isMapped = false;
        }
        return *this;
    }

    ~MappedBufferAccessor() {
        cleanup();
    }

    // Access the underlying accessor (const version)
    const AccessorType& get() const {
        return m_accessor;
    }

    // Access the underlying accessor (non-const version)
    AccessorType& get() {
        return m_accessor;
    }

    // Convenience operators for direct access
    const AccessorType* operator->() const {
        return &m_accessor;
    }

    AccessorType* operator->() {
        return &m_accessor;
    }

    const AccessorType& operator*() const {
        return m_accessor;
    }

    AccessorType& operator*() {
        return m_accessor;
    }

    // Check if accessor is valid
    bool isValid() const {
        return m_isMapped && m_buffer != nullptr;
    }

    explicit operator bool() const {
        return isValid();
    }

    // Manual early cleanup (optional - destructor will handle it anyway)
    void reset() {
        cleanup();
        m_buffer = nullptr;
        m_isMapped = false;
    }

protected:
    friend class BufferManager;

    // Only BufferManager (and derived classes) can construct this
    MappedBufferAccessor(Buffer* buffer, AccessorType&& accessor)
        : m_buffer(buffer)
        , m_accessor(std::move(accessor))
        , m_isMapped(true) {
    }

    void cleanup() {
        if (m_isMapped && m_buffer) {
            m_buffer->unmap();
            m_isMapped = false;
        }
    }

    Buffer* m_buffer = nullptr;
    AccessorType m_accessor;
    bool m_isMapped = false;
};