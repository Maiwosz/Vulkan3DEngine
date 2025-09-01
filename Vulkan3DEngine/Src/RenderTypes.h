#pragma once
#include <vulkan/vulkan.h>
#include <functional>

// Core enums for type identification
enum class RenderTemplateType : uint32_t {
    Forward = 0,
    UI = 1,
    Shadow = 2,
    // Add new types here
    Count
};

// Render metadata structure
struct RenderPassMetadata {
    VkExtent2D extent;
    VkFormat colorFormat;
    VkFormat depthFormat;
    VkSampleCountFlagBits samples;
    bool hasDepth;
    bool hasResolve;

    // Hash support
    bool operator==(const RenderPassMetadata& other) const {
        return extent.width == other.extent.width &&
            extent.height == other.extent.height &&
            colorFormat == other.colorFormat &&
            depthFormat == other.depthFormat &&
            samples == other.samples &&
            hasDepth == other.hasDepth &&
            hasResolve == other.hasResolve;
    }
};

// Cache key for render nodes
struct RenderNodeCacheKey {
    RenderTemplateType templateType;
    RenderPassMetadata metadata;

    bool operator==(const RenderNodeCacheKey& other) const {
        return templateType == other.templateType &&
            metadata == other.metadata;
    }
};

// Hash specialization for cache key
namespace std {
    template<> struct hash<RenderNodeCacheKey> {
        size_t operator()(const RenderNodeCacheKey& key) const {
            size_t hash = 0;
            hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.templateType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(key.metadata.extent.width) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(key.metadata.extent.height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.metadata.colorFormat)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.metadata.depthFormat)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.metadata.samples)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<bool>{}(key.metadata.hasDepth) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<bool>{}(key.metadata.hasResolve) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };
}