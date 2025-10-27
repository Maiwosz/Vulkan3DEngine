#pragma once
#include "BufferReader.h"
#include "MappedBufferAccessor.h"

// ============================================================================
// RAII WRAPPER - Automatically unmaps buffer on destruction
// ============================================================================

class MappedBufferReader : public MappedBufferAccessor<ShaderLib::BufferReader> {
public:
    MappedBufferReader() = default;

    // Inherit move semantics from base class
    using MappedBufferAccessor<ShaderLib::BufferReader>::MappedBufferAccessor;

    // Convenience accessor specifically for reader
    const ShaderLib::BufferReader& reader() const {
        return get();
    }

private:
    friend class BufferManager;

    // Only BufferManager can construct this
    MappedBufferReader(Buffer* buffer, ShaderLib::BufferReader&& reader)
        : MappedBufferAccessor<ShaderLib::BufferReader>(buffer, std::move(reader)) {
    }
};