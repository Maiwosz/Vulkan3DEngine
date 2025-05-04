#pragma once
#include "ShaderLib.h"
#include <string>

namespace ShaderLib {
    namespace TypeConversion {
        Stage StringToStage(const std::string& str);
        std::string StageToString(Stage stage);
        shaderc_shader_kind StageToShadercKind(Stage stage);
        DescriptorType SpirvTypeToDescriptorType(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& type);
        UniformType SPIRTypeToUniformType(const spirv_cross::SPIRType& type);
        uint32_t ComputeArraySize(const spirv_cross::SPIRType& type, const spirv_cross::Compiler& compiler);
        TypeInfo GetTypeInfo(UniformType type);
        std::string UniformTypeToString(UniformType type);
        UniformType StringToUniformType(const std::string& typeName);
    }
}
