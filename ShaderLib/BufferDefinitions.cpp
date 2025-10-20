#include "pch.h"
#include "BufferDefinitions.h"
#include "StandardBufferDefinitions.h"
#include "TypeConversions.h"

namespace ShaderLib {

    TypeInfo GetStructTypeInfo(const std::string& structName) {
        if (structName == "DirectionalLight") {
            return { sizeof(DirectionalLight), 16 };
        }
        else if (structName == "PointLight") {
            return { sizeof(PointLight), 16 };
        }
        else if (structName == "SpotLight") {
            return { sizeof(SpotLight), 16 };
        }

        return { 64, 16 };
    }

    // Get type info adjusted for layout standard
    TypeInfo GetTypeInfoForStandard(UniformType type, LayoutStandard standard) {
        if (standard == LayoutStandard::Std430) {
            return ShaderLib::TypeConversion::GetTypeInfoStd430(type);
        }
        return ShaderLib::TypeConversion::GetTypeInfo(type);
    }

    // Build the buffer from the builder
    BufferObject BufferBuilder::Build() {
        BufferObject buffer;
        buffer.name = name;
        buffer.set = set;
        buffer.binding = binding;
        buffer.bufferType = bufferType;
        buffer.layoutStandard = layoutStandard;

        uint32_t currentOffset = 0;

        for (const auto& field : fields) {
            UniformVariable var;
            var.name = field.name;
            var.type = field.type;
            var.typeName = field.typeName;
            var.arraySize = field.arraySize;

            if (field.isStruct) {
                TypeInfo structTypeInfo = GetStructTypeInfo(field.typeName);

                if (field.arraySize > 0) {
                    // Array of structs
                    var.type = UniformType::Array;

                    // Array element alignment depends on layout standard
                    uint32_t elementAlignment = GetArrayElementAlignment(structTypeInfo.alignment, layoutStandard);
                    uint32_t elementSize = AlignTo(structTypeInfo.size, elementAlignment);
                    var.size = field.arraySize * elementSize;

                    currentOffset = AlignTo(currentOffset, elementAlignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
                else {
                    // Single struct
                    var.type = UniformType::Struct;
                    var.size = structTypeInfo.size;

                    currentOffset = AlignTo(currentOffset, structTypeInfo.alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
            }
            else {
                // Basic types
                TypeInfo typeInfo = GetTypeInfoForStandard(var.type, layoutStandard);

                if (field.arraySize > 0) {
                    // Array of basic types
                    uint32_t elementAlignment = GetArrayElementAlignment(typeInfo.alignment, layoutStandard);
                    uint32_t elementSize = AlignTo(typeInfo.size, elementAlignment);
                    var.size = field.arraySize * elementSize;

                    currentOffset = AlignTo(currentOffset, elementAlignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
                else {
                    // Single basic type
                    var.size = typeInfo.size;

                    currentOffset = AlignTo(currentOffset, typeInfo.alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
            }

            buffer.variables.push_back(var);
        }

        // Set the total size - aligned to 16 bytes for UBO, 4 bytes for SSBO
        uint32_t finalAlignment = (bufferType == BufferType::Uniform) ? 16 : 4;
        buffer.size = AlignTo(currentOffset, finalAlignment);

        return buffer;
    }

    // Generate GLSL code for the buffer
    std::string BufferBuilder::GenerateGLSL() {
        std::stringstream ss;

        // Add struct definitions first
        for (const auto& structDef : structDefinitions) {
            ss << structDef;
        }

        // Determine layout keyword
        const char* layoutKeyword = (layoutStandard == LayoutStandard::Std140) ? "std140" : "std430";
        const char* bufferKeyword = (bufferType == BufferType::Uniform) ? "uniform" : "buffer";

        // Generate the buffer definition
        ss << "layout(" << layoutKeyword << ", set = " << set << ", binding = " << binding
            << ") " << bufferKeyword << " " << name << " {\n";

        for (const auto& field : fields) {
            ss << "    ";

            if (field.isStruct) {
                if (field.arraySize > 0) {
                    ss << field.typeName << " " << field.name << "[" << field.arraySize << "]";
                }
                else {
                    ss << field.typeName << " " << field.name;
                }
            }
            else {
                if (field.arraySize > 0) {
                    ss << TypeConversion::UniformTypeToString(field.type) << " " << field.name
                        << "[" << field.arraySize << "]";
                }
                else {
                    ss << TypeConversion::UniformTypeToString(field.type) << " " << field.name;
                }
            }

            if (!field.comment.empty()) {
                ss << "; // " << field.comment;
            }
            else {
                ss << ";";
            }

            ss << "\n";
        }

        ss << "};\n\n";

        return ss.str();
    }

    // Register a buffer definition in the registry
    void BufferRegistry::RegisterBuffer(const BufferObject& buffer) {
        buffers[buffer.name] = buffer;
    }

    // Get a registered buffer by name
    const BufferObject* BufferRegistry::GetBuffer(const std::string& name) const {
        auto it = buffers.find(name);
        if (it != buffers.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Generate GLSL code for a registered buffer
    std::string BufferRegistry::GenerateGLSL(const std::string& bufferName) const {
        const BufferObject* buffer = GetBuffer(bufferName);
        if (!buffer) {
            return "// Buffer '" + bufferName + "' not found in registry\n";
        }

        std::stringstream ss;

        // Check which struct definitions are needed
        bool hasDirectionalLight = false;
        bool hasPointLight = false;
        bool hasSpotLight = false;

        for (const auto& var : buffer->variables) {
            if (var.typeName == "DirectionalLight") hasDirectionalLight = true;
            else if (var.typeName == "PointLight") hasPointLight = true;
            else if (var.typeName == "SpotLight") hasSpotLight = true;
        }

        if (hasDirectionalLight) ss << DirectionalLight::GetGLSLDefinition();
        if (hasPointLight) ss << PointLight::GetGLSLDefinition();
        if (hasSpotLight) ss << SpotLight::GetGLSLDefinition();

        // Determine layout keyword
        const char* layoutKeyword = (buffer->layoutStandard == LayoutStandard::Std140) ? "std140" : "std430";
        const char* bufferKeyword = (buffer->bufferType == BufferType::Uniform) ? "uniform" : "buffer";

        // Generate the buffer definition
        ss << "layout(" << layoutKeyword << ", set = " << buffer->set << ", binding = " << buffer->binding
            << ") " << bufferKeyword << " " << buffer->name << " {\n";

        for (const auto& var : buffer->variables) {
            ss << "    ";

            if (var.type == UniformType::Struct) {
                ss << var.typeName << " " << var.name << ";";
            }
            else if (var.type == UniformType::Array && !var.typeName.empty()) {
                ss << var.typeName << " " << var.name << "[" << var.arraySize << "];";
            }
            else if (var.type == UniformType::Array) {
                ss << TypeConversion::UniformTypeToString(UniformType::Float) << " " << var.name
                    << "[" << var.arraySize << "];";
            }
            else {
                ss << TypeConversion::UniformTypeToString(var.type) << " " << var.name << ";";
            }

            ss << "\n";
        }

        ss << "};\n\n";

        return ss.str();
    }

    // Initialize standard buffers in the registry
    void BufferRegistry::InitializeStandardBuffers() {
        RegisterBuffer(CreateGlobalUBO());
        RegisterBuffer(CreateObjectUBO());
    }

    // Create GlobalUBO definition
    BufferObject BufferRegistry::CreateGlobalUBO() {
        return GLOBAL_UBO;
    }

    // Create ObjectUBO definition
    BufferObject BufferRegistry::CreateObjectUBO() {
        return OBJECT_UBO;
    }

} // namespace ShaderLib