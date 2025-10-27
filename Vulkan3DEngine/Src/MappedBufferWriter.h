#pragma once
#include "BufferWriter.h"
#include "MappedBufferAccessor.h"

// ============================================================================
// RAII WRAPPER - Automatically unmaps buffer on destruction
// ============================================================================

class MappedBufferWriter : public MappedBufferAccessor<ShaderLib::BufferWriter> {
public:
    MappedBufferWriter() = default;

    // Inherit move semantics from base class
    using MappedBufferAccessor<ShaderLib::BufferWriter>::MappedBufferAccessor;

    // Convenience accessor specifically for writer
    ShaderLib::BufferWriter& writer() {
        return get();
    }

    const ShaderLib::BufferWriter& writer() const {
        return get();
    }

private:
    friend class BufferManager;

    // Only BufferManager can construct this
    MappedBufferWriter(Buffer* buffer, ShaderLib::BufferWriter&& writer)
        : MappedBufferAccessor<ShaderLib::BufferWriter>(buffer, std::move(writer)) {
    }
};