#include "pch.h"
#include "ShaderParserPEGTL.h"
#include <algorithm>
#include <cctype>

namespace Shader {

    // Utility function to trim whitespace
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

    // Use directive - capture the name
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

    // Variable type
    template<>
    struct ShaderParserPEGTL::action<grammar::type_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedType_ = in.string();
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

    // Complete variable declaration
    template<>
    struct ShaderParserPEGTL::action<grammar::variable_decl> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            InputVariable var;
            var.type = parser.capturedType_;
            var.name = parser.capturedName_;
            var.isSampler = (var.type.find("sampler") != std::string::npos);

            parser.data_.inputVariables.push_back(var);

            // Clear captured state
            parser.capturedType_.clear();
            parser.capturedName_.clear();
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
            std::string code = in.string();

            // Trim leading/trailing whitespace but preserve internal structure
            code = trim(code);

            if (!code.empty()) {
                ShaderStage stage;
                stage.code = code;
                stage.stage = parser.currentStage_;
                parser.data_.stages.push_back(stage);
            }
        }
    };

    ParsedShaderData ShaderParserPEGTL::Parse(const std::string& source) {
        // Reset state
        data_ = ParsedShaderData();
        currentStage_ = ShaderLib::Stage::Vertex;
        capturedType_.clear();
        capturedName_.clear();

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