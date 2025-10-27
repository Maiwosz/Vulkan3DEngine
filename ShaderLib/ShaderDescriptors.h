#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>

namespace ShaderLib {

    // ============================================================================
    // DESCRIPTOR TYPES - What can be bound to shaders
    // ============================================================================

    enum class DescriptorType {
        // --- Samplers ---
        Sampler2D,          // Standard 2D texture
        Sampler2DArray,     // Texture array (e.g., shadow maps)
        SamplerCube,        // Cube map (skybox/environment)
        SamplerCubeArray,   // Cube map array

        // --- Shadow samplers ---
        Sampler2DShadow,    // Depth shadow map

        // --- Images (for compute/postprocess) ---
        Image2D,            // Writable image (compute shader)
        Image2DArray,       // Writable image array

        // --- Buffers ---
        UniformBuffer,      // Uniform buffer object (UBO)
        StorageBuffer,      // Storage buffer object (SSBO)

        // --- Separate sampler (Vulkan) ---
        Sampler,            // Separate sampler state object

        // --- Input attachment (for Vulkan deferred rendering) ---
        InputAttachment,    // Subpass input (G-buffer reading)

        // --- Fallback / default ---
        Unknown,
        COUNT
    };

    // ============================================================================
    // DESCRIPTOR CATEGORY - Main mutually exclusive categories
    // ============================================================================

    enum class DescriptorCategory : uint8_t {
        Texture,            // Combined image sampler (textures)
        Sampler,            // Separate sampler state object
        Image,              // Storage image (compute shader write)
        Buffer,             // Uniform/Storage buffer
        InputAttachment,    // Subpass input (Vulkan deferred)
        Unknown
    };

    // ============================================================================
    // DESCRIPTOR FLAGS - Additional properties that can be combined
    // ============================================================================

    enum class DescriptorFlags : uint32_t {
        None = 0,
        Array = 1 << 0,             // Is array type (e.g., sampler2DArray)
        Shadow = 1 << 1,            // Shadow sampler (depth comparison)
        Multisampled = 1 << 2,      // Multisampled texture
        RequiresFormat = 1 << 3,    // Requires explicit format (images)
        Cube = 1 << 4,              // Cube map
    };

    inline constexpr DescriptorFlags operator|(DescriptorFlags a, DescriptorFlags b) {
        return static_cast<DescriptorFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline constexpr DescriptorFlags operator&(DescriptorFlags a, DescriptorFlags b) {
        return static_cast<DescriptorFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline constexpr bool HasFlag(DescriptorFlags flags, DescriptorFlags flag) {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }

    // ============================================================================
    // DESCRIPTOR TYPE INFORMATION
    // ============================================================================

    struct DescriptorTypeInfo {
        DescriptorType type;
        const char* glslName;
        DescriptorCategory category;
        DescriptorFlags flags;
        uint8_t dimensions;         // 0 for buffers, 2 for 2D, 3 for 3D

        constexpr bool IsValid() const {
            return type != DescriptorType::Unknown;
        }

        constexpr bool IsTexture() const {
            return category == DescriptorCategory::Texture;
        }

        constexpr bool IsSampler() const {
            return category == DescriptorCategory::Sampler;
        }

        constexpr bool IsImage() const {
            return category == DescriptorCategory::Image;
        }

        constexpr bool IsBuffer() const {
            return category == DescriptorCategory::Buffer;
        }

        constexpr bool IsInputAttachment() const {
            return category == DescriptorCategory::InputAttachment;
        }

        constexpr bool IsArray() const {
            return HasFlag(flags, DescriptorFlags::Array);
        }

        constexpr bool IsShadow() const {
            return HasFlag(flags, DescriptorFlags::Shadow);
        }

        constexpr bool IsMultisampled() const {
            return HasFlag(flags, DescriptorFlags::Multisampled);
        }

        constexpr bool RequiresFormat() const {
            return HasFlag(flags, DescriptorFlags::RequiresFormat);
        }

        constexpr bool IsCube() const {
            return HasFlag(flags, DescriptorFlags::Cube);
        }

        constexpr bool IsCombinedImageSampler() const {
            return IsTexture();
        }
    };

    // ============================================================================
    // DESCRIPTOR TYPE TABLE
    // ============================================================================

#define DF DescriptorFlags
#define DC DescriptorCategory

    constexpr DescriptorTypeInfo DESCRIPTOR_TYPE_TABLE[] = {
        // type                            glslName              category                  flags                           dim

        // --- BASIC SAMPLERS ---
        { DescriptorType::Sampler2D,         "sampler2D",         DC::Texture,             DF::None,                       2 },
        { DescriptorType::SamplerCube,       "samplerCube",       DC::Texture,             DF::Cube,                       2 },
        { DescriptorType::Sampler2DArray,    "sampler2DArray",    DC::Texture,             DF::Array,                      2 },
        { DescriptorType::SamplerCubeArray,  "samplerCubeArray",  DC::Texture,             DF::Array | DF::Cube,           2 },

        // --- SHADOW SAMPLERS ---
        { DescriptorType::Sampler2DShadow,   "sampler2DShadow",   DC::Texture,             DF::Shadow,                     2 },

        // --- IMAGES (COMPUTE / POSTPROCESS) ---
        { DescriptorType::Image2D,           "image2D",           DC::Image,               DF::RequiresFormat,             2 },
        { DescriptorType::Image2DArray,      "image2DArray",      DC::Image,               DF::Array | DF::RequiresFormat, 2 },

        // --- BUFFERS ---
        { DescriptorType::UniformBuffer,     "uniform",           DC::Buffer,              DF::None,                       0 },
        { DescriptorType::StorageBuffer,     "buffer",            DC::Buffer,              DF::None,                       0 },

        // --- SAMPLER OBJECT (SEPARATE) ---
        { DescriptorType::Sampler,           "sampler",           DC::Sampler,             DF::None,                       0 },

        // --- INPUT ATTACHMENT (Vulkan Deferred) ---
        { DescriptorType::InputAttachment,   "subpassInput",      DC::InputAttachment,     DF::None,                       2 },

        // --- UNKNOWN ---
        { DescriptorType::Unknown,           "unknown",           DC::Unknown,             DF::None,                       0 },
    };

#undef DF
#undef DC

    static_assert(sizeof(DESCRIPTOR_TYPE_TABLE) / sizeof(DESCRIPTOR_TYPE_TABLE[0]) == static_cast<size_t>(DescriptorType::COUNT),
        "DESCRIPTOR_TYPE_TABLE must have an entry for each DescriptorType");

    // ============================================================================
    // DESCRIPTOR TYPE INFORMATION ACCESS
    // ============================================================================

    constexpr const DescriptorTypeInfo& GetDescriptorTypeInfo(DescriptorType type) {
        size_t index = static_cast<size_t>(type);
        return (index < static_cast<size_t>(DescriptorType::COUNT))
            ? DESCRIPTOR_TYPE_TABLE[index]
            : DESCRIPTOR_TYPE_TABLE[static_cast<size_t>(DescriptorType::Unknown)];
    }

    // ============================================================================
    // STRING CONVERSION
    // ============================================================================

    inline const char* DescriptorTypeToString(DescriptorType type) {
        return GetDescriptorTypeInfo(type).glslName;
    }

    inline DescriptorType StringToDescriptorType(const std::string& typeName) {
        static const std::unordered_map<std::string, DescriptorType> mapping = []() {
            std::unordered_map<std::string, DescriptorType> map;

            for (size_t i = 0; i < static_cast<size_t>(DescriptorType::COUNT); ++i) {
                const DescriptorTypeInfo& info = DESCRIPTOR_TYPE_TABLE[i];
                if (info.IsValid()) {
                    map[info.glslName] = info.type;
                }
            }

            return map;
            }();

        auto it = mapping.find(typeName);
        return (it != mapping.end()) ? it->second : DescriptorType::Unknown;
    }

    // ============================================================================
    // DESCRIPTOR CATEGORY HELPERS (for convenience)
    // ============================================================================

    inline bool IsTexture(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsTexture();
    }

    inline bool IsSampler(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsSampler();
    }

    inline bool IsImage(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsImage();
    }

    inline bool IsBuffer(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsBuffer();
    }

    inline bool IsShadowSampler(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsShadow();
    }

    inline bool IsCombinedImageSampler(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsCombinedImageSampler();
    }

    inline bool RequiresFormat(DescriptorType type) {
        return GetDescriptorTypeInfo(type).RequiresFormat();
    }

    inline bool IsArray(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsArray();
    }

    inline bool IsCube(DescriptorType type) {
        return GetDescriptorTypeInfo(type).IsCube();
    }

    inline DescriptorCategory GetDescriptorCategory(DescriptorType type) {
        return GetDescriptorTypeInfo(type).category;
    }

} // namespace ShaderLib