#pragma once
#include "ShaderLib.h"
#include <string>

namespace ShaderLib {
    namespace TypeConversion {
        // ============================================================================
        // STAGE CONVERSIONS
        // ============================================================================

        Stage StringToStage(const std::string& str);
        std::string StageToString(Stage stage);
        shaderc_shader_kind StageToShadercKind(Stage stage);

        // ============================================================================
        // SPIR-V TYPE CONVERSIONS
        // ============================================================================

        // Convert SPIR-V type to DescriptorType
        DescriptorType SpirvTypeToDescriptorType(
            const spirv_cross::Compiler& compiler,
            const spirv_cross::SPIRType& type
        );

        // Convert SPIR-V type to BaseType
        BaseType SPIRTypeToBaseType(const spirv_cross::SPIRType& type);

        // ============================================================================
        // STORAGE CLASS CONVERSIONS
        // ============================================================================

        DescriptorType StorageClassToDescriptorType(spv::StorageClass storageClass);

        // ============================================================================
        // ARRAY SIZE COMPUTATION
        // ============================================================================

        // Compute total array size in bytes for SPIR-V array types
        uint32_t ComputeArraySize(
            const spirv_cross::SPIRType& type,
            const spirv_cross::Compiler& compiler,
            LayoutStandard standard
        );

        // Calculate array size for BaseType arrays
        inline uint32_t CalculateArraySize(
            BaseType elementType,
            uint32_t arrayCount,
            LayoutStandard standard
        ) {
            const BaseTypeInfo& info = GetBaseTypeInfo(elementType);
            uint32_t elementSize = info.size;
            uint32_t baseAlignment = info.GetAlignment(standard);
            uint32_t alignment = GetArrayElementAlignment(baseAlignment, standard);
            uint32_t stride = AlignTo(elementSize, alignment);
            return stride * arrayCount;
        }

        // ============================================================================
        // BUFFER TYPE DETECTION
        // ============================================================================

        // Determine buffer type from storage class and layout
        inline BufferType StorageClassToBufferType(spv::StorageClass storageClass) {
            switch (storageClass) {
            case spv::StorageClassUniform:
                return BufferType::Uniform;
            case spv::StorageClassStorageBuffer:
                return BufferType::Storage;
            default:
                return BufferType::Uniform; // Default fallback
            }
        }

        // Determine layout standard from buffer type and SPIR-V decorations
        inline LayoutStandard BufferTypeToLayoutStandard(BufferType bufferType) {
            return (bufferType == BufferType::Storage)
                ? LayoutStandard::Std430
                : LayoutStandard::Std140;
        }

        // ============================================================================
        // BUFFER ACCESS MODE
        // ============================================================================

        inline const char* BufferAccessModeToString(BufferAccessMode mode) {
            switch (mode) {
            case BufferAccessMode::ReadOnly: return "readonly";
            case BufferAccessMode::WriteOnly: return "writeonly";
            case BufferAccessMode::ReadWrite: return "readwrite";
            default: return "unknown";
            }
        }
    }
}