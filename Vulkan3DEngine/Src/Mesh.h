#pragma once
#include "Handle.h"
#include <AssetLib.h>

struct Mesh {
    VramHandle vertexBuffer;
    VramHandle indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexStride;
    uint32_t attributes;
    uint8_t indexType;

    // Helper methods to check which attributes are present
    bool hasPosition() const {
        return (attributes & static_cast<uint32_t>(AssetLib::VertexAttribute::Position)) != 0;
    }

    bool hasNormal() const {
        return (attributes & static_cast<uint32_t>(AssetLib::VertexAttribute::Normal)) != 0;
    }

    bool hasTexCoord() const {
        return (attributes & static_cast<uint32_t>(AssetLib::VertexAttribute::TexCoord)) != 0;
    }

    bool hasTangent() const {
        return (attributes & static_cast<uint32_t>(AssetLib::VertexAttribute::Tangent)) != 0;
    }

    bool hasColor() const {
        return (attributes & static_cast<uint32_t>(AssetLib::VertexAttribute::Color)) != 0;
    }

    // Get index size in bytes
    size_t getIndexSize() const {
        return indexType == 0 ? sizeof(uint16_t) : sizeof(uint32_t);
    }
};