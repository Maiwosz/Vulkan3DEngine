#pragma once
#include "ShaderTypes.h"
#include "ShaderLib.h"
#include <string>
#include <vector>
#include <sstream>

namespace ShaderLib {

    // ============================================================================
    // BUFFER BUILDER
    // ============================================================================

    class BufferBuilder {
    private:
        std::string name;
        uint32_t set;
        uint32_t binding;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        std::vector<BufferVariable> variables;

    public:
        BufferBuilder(const std::string& bufferName, uint32_t descriptorSet, uint32_t bindingPoint)
            : name(bufferName), set(descriptorSet), binding(bindingPoint),
            bufferType(BufferType::Uniform), layoutStandard(LayoutStandard::Std140) {
        }

        BufferBuilder(const std::string& bufferName, uint32_t descriptorSet, uint32_t bindingPoint,
            BufferType type, LayoutStandard standard = LayoutStandard::Std140)
            : name(bufferName), set(descriptorSet), binding(bindingPoint),
            bufferType(type), layoutStandard(standard) {
            if (type == BufferType::Storage && standard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
        }

        BufferBuilder& SetBufferType(BufferType type) {
            bufferType = type;
            if (type == BufferType::Storage && layoutStandard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
            return *this;
        }

        BufferBuilder& SetLayoutStandard(LayoutStandard standard) {
            layoutStandard = standard;
            return *this;
        }

        // Add basic type field
        template<typename T>
        BufferBuilder& AddField(const std::string& fieldName) {
            static_assert(IsBaseTypeSupported<T>(), "Type not supported in buffers");

            const BaseTypeInfo& info = GetBaseTypeInfo(GetBaseTypeOf<T>());
            uint32_t offset = AlignTo(GetCurrentSize(), info.GetAlignment(layoutStandard));

            variables.emplace_back(fieldName, GetBaseTypeOf<T>(), info.size, offset);
            return *this;
        }

        // Add composite field (struct or array)
        BufferBuilder& AddCompositeField(const std::string& fieldName, std::shared_ptr<CompositeType> composite) {
            if (!composite) {
                throw std::invalid_argument("Composite type cannot be null");
            }

            uint32_t fieldAlignment = composite->GetAlignment();
            uint32_t offset = AlignTo(GetCurrentSize(), fieldAlignment);

            variables.emplace_back(fieldName, composite, offset);
            return *this;
        }

        BufferObject Build() {
            BufferObject buffer;
            buffer.name = name;
            buffer.set = set;
            buffer.binding = binding;
            buffer.bufferType = bufferType;
            buffer.layoutStandard = layoutStandard;
            buffer.variables = variables;

            // Calculate final size with proper alignment
            uint32_t finalAlignment = (bufferType == BufferType::Uniform) ? 16 : 4;
            buffer.size = AlignTo(GetCurrentSize(), finalAlignment);

            return buffer;
        }

        std::string GenerateGLSL() const {
            std::stringstream ss;

            // Collect unique composite type definitions
            std::vector<std::string> definitions;
            for (const auto& var : variables) {
                if (var.IsComposite()) {
                    std::string glsl = var.composite->GenerateGLSL();
                    if (!glsl.empty() && std::find(definitions.begin(), definitions.end(), glsl) == definitions.end()) {
                        definitions.push_back(glsl);
                    }
                }
            }

            // Add struct definitions
            for (const auto& def : definitions) {
                ss << def << "\n";
            }

            // Generate buffer definition
            const char* layoutKeyword = (layoutStandard == LayoutStandard::Std140) ? "std140" : "std430";
            const char* bufferKeyword = (bufferType == BufferType::Uniform) ? "uniform" : "buffer";

            ss << "layout(" << layoutKeyword << ", set = " << set << ", binding = " << binding
                << ") " << bufferKeyword << " " << name << " {\n";

            for (const auto& var : variables) {
                ss << "    " << var.GetTypeName() << " " << var.name << ";\n";
            }

            ss << "};\n";

            return ss.str();
        }

    private:
        uint32_t GetCurrentSize() const {
            if (variables.empty()) return 0;
            const auto& last = variables.back();
            return last.offset + last.size;
        }
    };

    // ============================================================================
    // GLSL GENERATION FOR BUFFER OBJECTS
    // ============================================================================

    inline std::string GenerateGLSL(const BufferObject& buffer) {
        std::stringstream ss;

        // Collect unique composite type definitions
        std::vector<std::string> definitions;
        for (const auto& var : buffer.variables) {
            if (var.IsComposite()) {
                std::string glsl = var.composite->GenerateGLSL();
                if (!glsl.empty() && std::find(definitions.begin(), definitions.end(), glsl) == definitions.end()) {
                    definitions.push_back(glsl);
                }
            }
        }

        // Add struct definitions
        for (const auto& def : definitions) {
            ss << def << "\n";
        }

        // Generate buffer definition
        const char* layoutKeyword = (buffer.layoutStandard == LayoutStandard::Std140) ? "std140" : "std430";
        const char* bufferKeyword = buffer.IsUniformBuffer() ? "uniform" : "buffer";

        ss << "layout(" << layoutKeyword << ", set = " << buffer.set << ", binding = " << buffer.binding
            << ") " << bufferKeyword << " " << buffer.name << " {\n";

        for (const auto& var : buffer.variables) {
            ss << "    " << var.GetTypeName() << " " << var.name << ";\n";
        }

        ss << "};\n";

        return ss.str();
    }

} // namespace ShaderLib