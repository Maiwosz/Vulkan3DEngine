#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <string_view>
#include "RenderTarget.h"

/**
 * Core type system for render graph architecture.
 *
 * Architecture Vision:
 * - Templates define static metadata and creation recipes in their own headers
 * - Instances are cached and reused based on template type + target type + dimensions
 * - Type-based template identification using template metaprogramming
 * - No runtime registration needed - all templates known at compile time
 */

 // Forward declaration to avoid circular dependency
class RenderTarget;

// Compile-time FNV-1a hash for string literals
namespace detail {
    constexpr size_t fnv1a_hash(std::string_view str) {
        constexpr size_t basis = 14695981039346656037ULL;
        constexpr size_t prime = 1099511628211ULL;
        size_t hash = basis;
        for (char c : str) {
            hash ^= static_cast<size_t>(c);
            hash *= prime;
        }
        return hash;
    }
}

// Forward declarations for SFINAE checks
template<typename T, typename = void>
struct has_static_name : std::false_type {};

template<typename T>
struct has_static_name<T, std::void_t<decltype(T::name)>> : std::true_type {};

// Base template type trait - all templates must specialize this
template<typename T>
struct TemplateTypeTraits;

// Base graph template type trait - all graph templates must specialize this
template<typename T>
struct GraphTemplateTypeTraits;

// Template metadata - now defined per template, not globally
struct RenderTemplateInfo {
    const char* name;

    // Capability flags
    bool requiresDepthBuffer;
    bool requiresColorBuffer;
    bool supportsMSAA;

    // Preferred formats - templates can override these based on render target
    VkFormat preferredColorFormat;
    VkFormat preferredDepthFormat;

    // Compile-time constructor for static initialization
    constexpr RenderTemplateInfo(
        const char* n,
        bool needsDepth = true,
        bool needsColor = true,
        bool msaa = true,
        VkFormat colorFmt = VK_FORMAT_R8G8B8A8_UNORM,
        VkFormat depthFmt = VK_FORMAT_D32_SFLOAT
    ) : name(n), requiresDepthBuffer(needsDepth), requiresColorBuffer(needsColor),
        supportsMSAA(msaa), preferredColorFormat(colorFmt), preferredDepthFormat(depthFmt) {
    }
};

// Graph template metadata - minimal interface
struct RenderGraphTemplateInfo {
    const char* name;

    constexpr RenderGraphTemplateInfo(const char* n) : name(n) {}
};

// Cache key for render node instances - uses template type instead of enum
template<typename TemplateType>
struct RenderNodeCacheKey {
    using template_type = TemplateType;
    RenderTarget::Type targetType;
    VkExtent2D extent;

    bool operator==(const RenderNodeCacheKey& other) const {
        return targetType == other.targetType &&
            extent.width == other.extent.width &&
            extent.height == other.extent.height;
    }
};

// Cache key for graph instances - now type-based
template<typename TemplateType>
struct RenderGraphCacheKey {
    using template_type = TemplateType;
    RenderTarget::Type targetType;
    VkExtent2D extent;

    bool operator==(const RenderGraphCacheKey& other) const {
        return targetType == other.targetType &&
            extent.width == other.extent.width &&
            extent.height == other.extent.height;
    }
};

// Hash for render node cache key - uses template type traits for consistent hashing
template<typename TemplateType>
struct std::hash<RenderNodeCacheKey<TemplateType>> {
    size_t operator()(const RenderNodeCacheKey<TemplateType>& key) const {
        size_t hash = 0;
        hash ^= TemplateTypeTraits<TemplateType>::type_hash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(static_cast<int>(key.targetType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(key.extent.width) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(key.extent.height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

// Hash specialization for graph cache key
template<typename TemplateType>
struct std::hash<RenderGraphCacheKey<TemplateType>> {
    size_t operator()(const RenderGraphCacheKey<TemplateType>& key) const {
        size_t hash = 0;
        hash ^= GraphTemplateTypeTraits<TemplateType>::type_hash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(static_cast<int>(key.targetType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(key.extent.width) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(key.extent.height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

// Helper to get template info at compile time
template<typename TemplateType>
constexpr const RenderTemplateInfo& getTemplateInfo() {
    return TemplateTypeTraits<TemplateType>::getStaticTemplateInfo();
}

// Helper to get template name at compile time
template<typename TemplateType>
constexpr const char* getTemplateName() {
    return TemplateTypeTraits<TemplateType>::name;
}

// Helper to get graph template info at compile time
template<typename GraphTemplateType>
constexpr const RenderGraphTemplateInfo& getGraphTemplateInfo() {
    return GraphTemplateTypeTraits<GraphTemplateType>::getStaticTemplateInfo();
}

// Helper to get graph template name at compile time
template<typename GraphTemplateType>
constexpr const char* getGraphTemplateName() {
    return GraphTemplateTypeTraits<GraphTemplateType>::name;
}