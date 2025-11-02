#include "pch.h"
#include "ShaderParserPEGTL.h"
#include <algorithm>
#include <cctype>

namespace Shader {

    static std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
            return std::isspace(ch);
            });
        auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
            return std::isspace(ch);
            }).base();
        return (start < end) ? std::string(start, end) : std::string();
    }

    // Version directive
    template<>
    struct ShaderParserPEGTL::action<grammar::version_directive> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.data_.versionLine = trim(in.string());
        }
    };

    // Use directive
    template<>
    struct ShaderParserPEGTL::action<grammar::use_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            std::string name = in.string();
            if (name == "global_ubo") {
                parser.data_.usesGlobalUBO = true;
            }
            else if (name == "object_ubo") {
                parser.data_.usesObjectUBO = true;
            }
        }
    };

    // Type name
    template<>
    struct ShaderParserPEGTL::action<grammar::type_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedType_ = in.string();
            parser.capturedIsArray_ = false;
            parser.capturedArraySize_ = 0;
        }
    };

    // Array size
    template<>
    struct ShaderParserPEGTL::action<grammar::array_size> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedIsArray_ = true;
            parser.capturedArraySize_ = std::stoul(in.string());
        }
    };

    // Variable name
    template<>
    struct ShaderParserPEGTL::action<grammar::var_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedName_ = in.string();
        }
    };

    // Struct name
    template<>
    struct ShaderParserPEGTL::action<grammar::struct_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentStruct_.name = in.string();
            parser.currentStruct_.fields.clear();
            parser.parsingStruct_ = true;
        }
    };

    // Struct field
    template<>
    struct ShaderParserPEGTL::action<grammar::struct_field> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (!parser.parsingStruct_) return;

            StructField field;
            field.name = parser.capturedName_;
            field.type.baseType = parser.capturedType_;
            field.type.isArray = parser.capturedIsArray_;
            field.type.arraySize = parser.capturedArraySize_;

            // Check if it's a struct type
            const auto* structDef = parser.data_.FindStruct(parser.capturedType_);
            if (structDef) {
                field.type.isStruct = true;
                field.type.structDef = std::make_shared<StructDefinition>(*structDef);
            }

            parser.currentStruct_.fields.push_back(field);

            parser.capturedType_.clear();
            parser.capturedName_.clear();
            parser.capturedIsArray_ = false;
            parser.capturedArraySize_ = 0;
        }
    };

    // Struct definition complete
    template<>
    struct ShaderParserPEGTL::action<grammar::struct_definition> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.data_.structDefinitions.push_back(parser.currentStruct_);
            parser.parsingStruct_ = false;
        }
    };

    // Variable declaration
    template<>
    struct ShaderParserPEGTL::action<grammar::variable_decl> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (parser.parsingStruct_) return; // Handled by struct_field

            InputVariable var;
            var.name = parser.capturedName_;
            var.isSampler = (parser.capturedType_.find("sampler") != std::string::npos);

            // Build TypeInfo
            var.typeInfo.baseType = parser.capturedType_;
            var.typeInfo.isArray = parser.capturedIsArray_;
            var.typeInfo.arraySize = parser.capturedArraySize_;

            // Check if it's a struct type
            const auto* structDef = parser.data_.FindStruct(parser.capturedType_);
            if (structDef) {
                var.typeInfo.isStruct = true;
                var.typeInfo.structDef = std::make_shared<StructDefinition>(*structDef);
            }

            // Set legacy type field for compatibility
            var.type = parser.capturedType_;
            if (parser.capturedIsArray_) {
                var.type += "[" + std::to_string(parser.capturedArraySize_) + "]";
            }

            // Add to appropriate vector
            switch (parser.currentStructType_) {
            case ShaderDataStructType::Input:
                parser.data_.inputVariables.push_back(var);
                break;
            case ShaderDataStructType::Output:
                parser.data_.outputVariables.push_back(var);
                break;
            case ShaderDataStructType::InputOutput:
                parser.data_.inputOutputVariables.push_back(var);
                break;
            case ShaderDataStructType::Samplers:
                parser.data_.samplerVariables.push_back(var);
                break;
            }

            parser.capturedType_.clear();
            parser.capturedName_.clear();
            parser.capturedIsArray_ = false;
            parser.capturedArraySize_ = 0;
        }
    };

    // Stage name
    template<>
    struct ShaderParserPEGTL::action<grammar::stage_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            std::string name = in.string();

            if (name == "vertex") {
                parser.currentStage_ = ShaderLib::Stage::Vertex;
            }
            else if (name == "fragment") {
                parser.currentStage_ = ShaderLib::Stage::Fragment;
            }
            else if (name == "compute") {
                parser.currentStage_ = ShaderLib::Stage::Compute;
            }
            else if (name == "geometry") {
                parser.currentStage_ = ShaderLib::Stage::Geometry;
            }
            else if (name == "tessellation_control" || name == "tess_control") {
                parser.currentStage_ = ShaderLib::Stage::TessellationControl;
            }
            else if (name == "tessellation_evaluation" || name == "tess_eval") {
                parser.currentStage_ = ShaderLib::Stage::TessellationEvaluation;
            }
        }
    };

    // Stage code
    template<>
    struct ShaderParserPEGTL::action<grammar::stage_code> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            std::string code = trim(in.string());

            if (!code.empty()) {
                ShaderStage stage;
                stage.code = code;
                stage.stage = parser.currentStage_;
                parser.data_.stages.push_back(stage);
            }
        }
    };

    // Data struct type keywords
    template<>
    struct ShaderParserPEGTL::action<grammar::input_data_keyword> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentStructType_ = ShaderDataStructType::Input;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::output_data_keyword> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentStructType_ = ShaderDataStructType::Output;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::input_output_data_keyword> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentStructType_ = ShaderDataStructType::InputOutput;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::samplers_keyword> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentStructType_ = ShaderDataStructType::Samplers;
        }
    };

    ParsedShaderData ShaderParserPEGTL::Parse(const std::string& source) {
        // Reset state
        data_ = ParsedShaderData();
        currentStage_ = ShaderLib::Stage::Vertex;
        capturedType_.clear();
        capturedName_.clear();
        capturedIsArray_ = false;
        capturedArraySize_ = 0;
        parsingStruct_ = false;

        try {
            tao::pegtl::memory_input input(source, "shader");

            if (!tao::pegtl::parse<grammar::grammar, action>(input, *this)) {
                throw std::runtime_error("Failed to parse shader source");
            }

            if (data_.stages.empty()) {
                throw std::runtime_error("No shader stages found");
            }

            return data_;
        }
        catch (const tao::pegtl::parse_error& e) {
            throw std::runtime_error(std::string("Parse error at line ") +
                std::to_string(e.positions().front().line) +
                ": " + e.what());
        }
    }

} // namespace Shader