#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace Graphics {
    // Enumeratory agnostyczne
    enum class BufferUsageType {
        Staging,
        Vertex,
        Index,
        Uniform,
        Storage,
        Indirect,
        TransferSrc,
        TransferDst,
        UniformTexel,
        StorageTexel,
        ConditionalRendering
    };

    enum class ImageFormat {
        // 8-bit formats
        R8_UNORM,
        R8_SRGB,
        R8G8B8A8_UNORM,
        R8G8B8A8_SRGB,
        B8G8R8A8_UNORM,
        B8G8R8A8_SRGB,

        // 16-bit formats
        R16_SFLOAT,
        R16G16B16A16_SFLOAT,

        // 32-bit formats
        R32_SFLOAT,
        R32G32B32A32_SFLOAT,
        D32_SFLOAT,

        // Compressed formats
        BC7_UNORM,
        BC7_SRGB
    };

    enum class ImageUsage {
        Sampled = 1 << 0,
        Storage = 1 << 1,
        DepthStencil = 1 << 2,
        TransferSrc = 1 << 3,
        TransferDst = 1 << 4,
        ColorAttachment = 1 << 5,
        Transient = 1 << 6,
        InputAttachment = 1 << 7
    };

    // Operatory bitowe dla ImageUsage
    inline ImageUsage operator|(ImageUsage a, ImageUsage b) noexcept {
        return static_cast<ImageUsage>(static_cast<int>(a) | static_cast<int>(b));
    }

    // Struktury konfiguracyjne
    struct BufferCreateInfo {
        BufferUsageType usage;
        size_t size;
    };

    struct ImageCreateInfo {
        uint32_t width;
        uint32_t height;
        ImageFormat format;
        ImageUsage usage;
        uint32_t mipLevels = 1;
        Settings::MsaaSampleCount samples = Settings::MsaaSampleCount::Samples1;
    };

    // Konwersje do Vulkanowych typów
    inline VkBufferUsageFlags convertBufferUsage(Graphics::BufferUsageType usage) {
        switch (usage) {
        case BufferUsageType::Staging:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferUsageType::Vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferUsageType::Index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferUsageType::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferUsageType::Storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsageType::Indirect:
            return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        case BufferUsageType::TransferSrc:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferUsageType::TransferDst:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferUsageType::UniformTexel:
            return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        case BufferUsageType::StorageTexel:
            return VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        case BufferUsageType::ConditionalRendering:
            return VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
        default:
            throw std::runtime_error("Unknown buffer usage type");
        }
    }

    inline VkFormat convertImageFormat(Graphics::ImageFormat format) {
        switch (format) {
        case ImageFormat::R8_UNORM:          return VK_FORMAT_R8_UNORM;
        case ImageFormat::R8_SRGB:           return VK_FORMAT_R8_SRGB;
        case ImageFormat::R8G8B8A8_UNORM:    return VK_FORMAT_R8G8B8A8_UNORM;
        case ImageFormat::R8G8B8A8_SRGB:     return VK_FORMAT_R8G8B8A8_SRGB;
        case ImageFormat::B8G8R8A8_UNORM:    return VK_FORMAT_B8G8R8A8_UNORM;
        case ImageFormat::B8G8R8A8_SRGB:     return VK_FORMAT_B8G8R8A8_SRGB;
        case ImageFormat::R16_SFLOAT:        return VK_FORMAT_R16_SFLOAT;
        case ImageFormat::R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ImageFormat::R32_SFLOAT:        return VK_FORMAT_R32_SFLOAT;
        case ImageFormat::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case ImageFormat::D32_SFLOAT:        return VK_FORMAT_D32_SFLOAT;
        case ImageFormat::BC7_UNORM:         return VK_FORMAT_BC7_UNORM_BLOCK;
        case ImageFormat::BC7_SRGB:          return VK_FORMAT_BC7_SRGB_BLOCK;
        default:
            throw std::runtime_error("Unsupported image format");
        }
    }

    inline VkImageUsageFlags convertImageUsage(Graphics::ImageUsage usage) {
        VkImageUsageFlags flags = 0;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::Sampled))
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::Storage))
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::DepthStencil))
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::TransferSrc))
            flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::TransferDst))
            flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::ColorAttachment))
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::Transient))
            flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        if (static_cast<int>(usage) & static_cast<int>(ImageUsage::InputAttachment))
            flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        return flags;
    }

    // Konwersje z Vulkanowych typów
    inline BufferUsageType convertVkBufferUsage(VkBufferUsageFlags flags) {
        if (flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)         return BufferUsageType::TransferSrc;
        if (flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT)         return BufferUsageType::TransferDst;
        if (flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)        return BufferUsageType::Vertex;
        if (flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)         return BufferUsageType::Index;
        if (flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)       return BufferUsageType::Uniform;
        if (flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)       return BufferUsageType::Storage;
        if (flags & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)      return BufferUsageType::Indirect;
        if (flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) return BufferUsageType::UniformTexel;
        if (flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) return BufferUsageType::StorageTexel;
        if (flags & VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT) return BufferUsageType::ConditionalRendering;

        throw std::runtime_error("Unsupported VkBufferUsageFlags");
    }

    inline ImageFormat convertVkFormat(VkFormat format) {
        switch (format) {
        case VK_FORMAT_R8_UNORM:              return ImageFormat::R8_UNORM;
        case VK_FORMAT_R8_SRGB:               return ImageFormat::R8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:        return ImageFormat::R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:        return ImageFormat::R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:        return ImageFormat::B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:         return ImageFormat::B8G8R8A8_SRGB;
        case VK_FORMAT_R16_SFLOAT:            return ImageFormat::R16_SFLOAT;
        case VK_FORMAT_R16G16B16A16_SFLOAT:   return ImageFormat::R16G16B16A16_SFLOAT;
        case VK_FORMAT_R32_SFLOAT:            return ImageFormat::R32_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:  return ImageFormat::R32G32B32A32_SFLOAT;
        case VK_FORMAT_D32_SFLOAT:            return ImageFormat::D32_SFLOAT;
        case VK_FORMAT_BC7_UNORM_BLOCK:       return ImageFormat::BC7_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK:        return ImageFormat::BC7_SRGB;
        default:
            throw std::runtime_error("Unsupported VkFormat");
        }
    }

    inline ImageUsage convertVkImageUsage(VkImageUsageFlags flags) {
        ImageUsage usage = static_cast<ImageUsage>(0);
        if (flags & VK_IMAGE_USAGE_SAMPLED_BIT)           usage = usage | ImageUsage::Sampled;
        if (flags & VK_IMAGE_USAGE_STORAGE_BIT)           usage = usage | ImageUsage::Storage;
        if (flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) usage = usage | ImageUsage::DepthStencil;
        if (flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)       usage = usage | ImageUsage::TransferSrc;
        if (flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)       usage = usage | ImageUsage::TransferDst;
        if (flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)   usage = usage | ImageUsage::ColorAttachment;
        if (flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) usage = usage | ImageUsage::Transient;
        if (flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)   usage = usage | ImageUsage::InputAttachment;
        return usage;
    }

    inline VkSampleCountFlagBits convertSampleCount(Settings::MsaaSampleCount samples) {
        switch (samples) {
        case Settings::MsaaSampleCount::Samples1: return VK_SAMPLE_COUNT_1_BIT;
        case Settings::MsaaSampleCount::Samples2: return VK_SAMPLE_COUNT_2_BIT;
        case Settings::MsaaSampleCount::Samples4: return VK_SAMPLE_COUNT_4_BIT;
        case Settings::MsaaSampleCount::Samples8: return VK_SAMPLE_COUNT_8_BIT;
        case Settings::MsaaSampleCount::Samples16: return VK_SAMPLE_COUNT_16_BIT;
        case Settings::MsaaSampleCount::Samples32: return VK_SAMPLE_COUNT_32_BIT;
        case Settings::MsaaSampleCount::Samples64: return VK_SAMPLE_COUNT_64_BIT;
        default:
            throw std::runtime_error("Unsupported sample count");
        }
    }

    inline Settings::MsaaSampleCount convertVkSampleCount(VkSampleCountFlagBits samples) {
        switch (samples) {
        case VK_SAMPLE_COUNT_1_BIT: return Settings::MsaaSampleCount::Samples1;
        case VK_SAMPLE_COUNT_2_BIT: return Settings::MsaaSampleCount::Samples2;
        case VK_SAMPLE_COUNT_4_BIT: return Settings::MsaaSampleCount::Samples4;
        case VK_SAMPLE_COUNT_8_BIT: return Settings::MsaaSampleCount::Samples8;
        case VK_SAMPLE_COUNT_16_BIT: return Settings::MsaaSampleCount::Samples16;
        case VK_SAMPLE_COUNT_32_BIT: return Settings::MsaaSampleCount::Samples32;
        case VK_SAMPLE_COUNT_64_BIT: return Settings::MsaaSampleCount::Samples64;
        default:
            throw std::runtime_error("Unsupported VkSampleCountFlagBits");
        }
    }

    inline VkDeviceSize calculateImageSize(VkFormat format, uint32_t width, uint32_t height)
    {
        // Helper functions for block-compressed formats
        auto calculateBlockCompressedSize = [](uint32_t width, uint32_t height, uint32_t blockSize) {
            uint32_t blockWidth = (width + 3) / 4;
            uint32_t blockHeight = (height + 3) / 4;
            return static_cast<VkDeviceSize>(blockWidth) * blockHeight * blockSize;
            };

        // Helper for ETC/EAC formats which use 4x4 blocks
        auto calculateETCSize = [](uint32_t width, uint32_t height, uint32_t blockSize) {
            uint32_t blockWidth = (width + 3) / 4;
            uint32_t blockHeight = (height + 3) / 4;
            return static_cast<VkDeviceSize>(blockWidth) * blockHeight * blockSize;
            };

        // Helper for ASTC formats which use variable-sized blocks
        auto calculateASTCSize = [](uint32_t width, uint32_t height, uint32_t blockWidth, uint32_t blockHeight) {
            uint32_t blocksX = (width + blockWidth - 1) / blockWidth;
            uint32_t blocksY = (height + blockHeight - 1) / blockHeight;
            return static_cast<VkDeviceSize>(blocksX) * blocksY * 16; // ASTC always uses 128 bits (16 bytes) per block
            };

        // Handle all formats
        switch (format) {
            // 8-bit formats
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
            return static_cast<VkDeviceSize>(width) * height;

            // 16-bit formats (8-bit × 2 components or 16-bit × 1 component)
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 2;

            // 24-bit formats (8-bit × 3 components)
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:
            return static_cast<VkDeviceSize>(width) * height * 3;

            // 32-bit formats (8-bit × 4 components or 16-bit × 2 components or 32-bit × 1 component)
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return static_cast<VkDeviceSize>(width) * height * 4;

            // 48-bit formats (16-bit × 3 components)
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 6;

            // 64-bit formats (16-bit × 4 components or 32-bit × 2 components)
        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return static_cast<VkDeviceSize>(width) * height * 8;

            // 96-bit formats (32-bit × 3 components)
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 12;

            // 128-bit formats (32-bit × 4 components)
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 16;

            // 64-bit formats (64-bit × 1 component)
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 8;

            // 128-bit formats (64-bit × 2 components)
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 16;

            // 192-bit formats (64-bit × 3 components)
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 24;

            // 256-bit formats (64-bit × 4 components)
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            return static_cast<VkDeviceSize>(width) * height * 32;

            // BC1 formats (DXT1) - 4x4 blocks, 8 bytes per block
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return calculateBlockCompressedSize(width, height, 8);

            // BC2 formats (DXT3) - 4x4 blocks, 16 bytes per block
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
            return calculateBlockCompressedSize(width, height, 16);

            // BC3 formats (DXT5) - 4x4 blocks, 16 bytes per block
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return calculateBlockCompressedSize(width, height, 16);

            // BC4 formats - 4x4 blocks, 8 bytes per block
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
            return calculateBlockCompressedSize(width, height, 8);

            // BC5 formats - 4x4 blocks, 16 bytes per block
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
            return calculateBlockCompressedSize(width, height, 16);

            // BC6H formats - 4x4 blocks, 16 bytes per block
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            return calculateBlockCompressedSize(width, height, 16);

            // BC7 formats - 4x4 blocks, 16 bytes per block
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return calculateBlockCompressedSize(width, height, 16);

            // ETC2/EAC formats - 4x4 blocks
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            return calculateETCSize(width, height, 8);

        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            return calculateETCSize(width, height, 8);

        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            return calculateETCSize(width, height, 16);

            // EAC formats
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:
            return calculateETCSize(width, height, 8);

        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
            return calculateETCSize(width, height, 16);

            // ASTC formats - all use 16 bytes per block but with different block sizes
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            return calculateASTCSize(width, height, 4, 4);

        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            return calculateASTCSize(width, height, 5, 4);

        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            return calculateASTCSize(width, height, 5, 5);

        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            return calculateASTCSize(width, height, 6, 5);

        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            return calculateASTCSize(width, height, 6, 6);

        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            return calculateASTCSize(width, height, 8, 5);

        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            return calculateASTCSize(width, height, 8, 6);

        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            return calculateASTCSize(width, height, 8, 8);

        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            return calculateASTCSize(width, height, 10, 5);

        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            return calculateASTCSize(width, height, 10, 6);

        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            return calculateASTCSize(width, height, 10, 8);

        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            return calculateASTCSize(width, height, 10, 10);

        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            return calculateASTCSize(width, height, 12, 10);

        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return calculateASTCSize(width, height, 12, 12);

            // PVRTC formats
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
            return (std::max(width, 16u) * std::max(height, 8u)) / 4; // 2 bits per pixel

        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
            return (std::max(width, 8u) * std::max(height, 8u)) / 2; // 4 bits per pixel

        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
            return (std::max(width, 16u) * std::max(height, 8u)) / 4; // 2 bits per pixel

        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
            return (std::max(width, 8u) * std::max(height, 8u)) / 2; // 4 bits per pixel

            // YCbCr formats
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
            return static_cast<VkDeviceSize>(width) * height * 2; // 4 components for 2 pixels

        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            return static_cast<VkDeviceSize>(width) * height * 3 / 2; // Y plane + half-size U/V planes

        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
            return static_cast<VkDeviceSize>(width) * height * 2; // Y plane + half-width U/V planes

        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
            return static_cast<VkDeviceSize>(width) * height * 3; // Full-size Y, U, and V planes

        case VK_FORMAT_R10X6_UNORM_PACK16:
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
            // 10-bit components packed in 16-bit
            return static_cast<VkDeviceSize>(width) * height * 2;

        case VK_FORMAT_R12X4_UNORM_PACK16:
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
            // 12-bit components packed in 16-bit
            return static_cast<VkDeviceSize>(width) * height * 2;

        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
            // 16-bit components
            return static_cast<VkDeviceSize>(width) * height * 4;

        default:
            throw std::runtime_error("Unsupported image format for data upload: " + std::to_string(format));
        }
    }
}