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

    // ========================================================================
    // HELPER: CreateAccessPatterns z wartościami domyślnymi
    // ========================================================================

    ShaderLib::BufferAccessPatterns BufferDefinition::CreateAccessPatterns() const {
        using namespace ShaderLib;

        // Pobierz wartości domyślne dla danego typu bufora
        BufferAccessPatterns defaultPatterns =
            (bufferType == BufferType::Uniform)
            ? BufferAccessPatterns::UniformBuffer()
            : BufferAccessPatterns::StorageBuffer();

        ProcessorAccessProfile cpuProfile = defaultPatterns.cpuAccess;
        ProcessorAccessProfile gpuProfile = defaultPatterns.gpuAccess;

        // Nadpisz wartościami z parsowania (jeśli podane)
        if (cpuAccess.frequency.has_value()) {
            cpuProfile.frequency = cpuAccess.frequency.value();
        }
        if (cpuAccess.operation.has_value()) {
            cpuProfile.operation = cpuAccess.operation.value();
        }
        if (cpuAccess.size.has_value()) {
            cpuProfile.size = cpuAccess.size.value();
        }

        if (gpuAccess.frequency.has_value()) {
            gpuProfile.frequency = gpuAccess.frequency.value();
        }
        if (gpuAccess.operation.has_value()) {
            gpuProfile.operation = gpuAccess.operation.value();
        }
        if (gpuAccess.size.has_value()) {
            gpuProfile.size = gpuAccess.size.value();
        }

        return BufferAccessPatterns(cpuProfile, gpuProfile);
    }

    // ========================================================================
    // EXISTING ACTIONS
    // ========================================================================

    template<>
    struct ShaderParserPEGTL::action<grammar::version_directive> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.data_.versionLine = trim(in.string());
        }
    };

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

    template<>
    struct ShaderParserPEGTL::action<grammar::type_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedType_ = in.string();
            parser.capturedIsArray_ = false;
            parser.capturedArraySize_ = 0;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::array_size> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedIsArray_ = true;
            parser.capturedArraySize_ = std::stoul(in.string());
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::var_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturedName_ = in.string();
        }
    };

    // ========================================================================
    // STRUCT PARSING
    // ========================================================================

    template<>
    struct ShaderParserPEGTL::action<grammar::struct_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (parser.parsingBuffer_) {
                // Ignore - we're in a buffer definition
                return;
            }
            parser.currentStruct_.name = in.string();
            parser.currentStruct_.fields.clear();
            parser.parsingStruct_ = true;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::struct_field> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
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

            if (parser.parsingStruct_) {
                parser.currentStruct_.fields.push_back(field);
            }
            else if (parser.parsingBuffer_) {
                parser.currentBuffer_.fields.push_back(field);
            }

            parser.capturedType_.clear();
            parser.capturedName_.clear();
            parser.capturedIsArray_ = false;
            parser.capturedArraySize_ = 0;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::struct_definition> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (parser.parsingStruct_) {
                parser.data_.structDefinitions.push_back(parser.currentStruct_);
                parser.parsingStruct_ = false;
            }
        }
    };

    // ========================================================================
    // BUFFER PARSING ACTIONS
    // ========================================================================

    template<>
    struct ShaderParserPEGTL::action<grammar::buffer_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.currentBuffer_ = BufferDefinition();
            parser.currentBuffer_.name = in.string();
            parser.parsingBuffer_ = true;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::buffer_type> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            std::string typeStr = in.string();
            if (typeStr == "uniform") {
                parser.currentBuffer_.bufferType = ShaderLib::BufferType::Uniform;
            }
            else if (typeStr == "storage") {
                parser.currentBuffer_.bufferType = ShaderLib::BufferType::Storage;
            }
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::processor_keyword_gpu> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturingGPUAccess_ = true;
            parser.capturingCPUAccess_ = false;
            parser.currentAccessSpec_ = &parser.currentBuffer_.gpuAccess;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::processor_keyword_cpu> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.capturingCPUAccess_ = true;
            parser.capturingGPUAccess_ = false;
            parser.currentAccessSpec_ = &parser.currentBuffer_.cpuAccess;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::access_frequency> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (!parser.currentAccessSpec_) return;

            std::string freqStr = in.string();
            using namespace ShaderLib;

            if (freqStr == "Never") {
                parser.currentAccessSpec_->frequency = AccessFrequency::Never;
            }
            else if (freqStr == "OneTime") {
                parser.currentAccessSpec_->frequency = AccessFrequency::OneTime;
            }
            else if (freqStr == "EveryFewFrames") {
                parser.currentAccessSpec_->frequency = AccessFrequency::EveryFewFrames;
            }
            else if (freqStr == "OncePerFrame") {
                parser.currentAccessSpec_->frequency = AccessFrequency::OncePerFrame;
            }
            else if (freqStr == "MultiplePerFrame") {
                parser.currentAccessSpec_->frequency = AccessFrequency::MultiplePerFrame;
            }
            else if (freqStr == "Continuous") {
                parser.currentAccessSpec_->frequency = AccessFrequency::Continuous;
            }
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::access_operation> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (!parser.currentAccessSpec_) return;

            std::string opStr = in.string();
            using namespace ShaderLib;

            if (opStr == "None") {
                parser.currentAccessSpec_->operation = AccessOperation::None;
            }
            else if (opStr == "ReadOnly") {
                parser.currentAccessSpec_->operation = AccessOperation::ReadOnly;
            }
            else if (opStr == "WriteOnly") {
                parser.currentAccessSpec_->operation = AccessOperation::WriteOnly;
            }
            else if (opStr == "ReadWrite") {
                parser.currentAccessSpec_->operation = AccessOperation::ReadWrite;
            }
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::access_size> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (!parser.currentAccessSpec_) return;

            std::string sizeStr = in.string();
            using namespace ShaderLib;

            if (sizeStr == "None") {
                parser.currentAccessSpec_->size = AccessSize::None;
            }
            else if (sizeStr == "Tiny") {
                parser.currentAccessSpec_->size = AccessSize::Tiny;
            }
            else if (sizeStr == "Small") {
                parser.currentAccessSpec_->size = AccessSize::Small;
            }
            else if (sizeStr == "Medium") {
                parser.currentAccessSpec_->size = AccessSize::Medium;
            }
            else if (sizeStr == "Large") {
                parser.currentAccessSpec_->size = AccessSize::Large;
            }
            else if (sizeStr == "VeryLarge") {
                parser.currentAccessSpec_->size = AccessSize::VeryLarge;
            }
            else if (sizeStr == "Massive") {
                parser.currentAccessSpec_->size = AccessSize::Massive;
            }
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::buffer_definition> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.data_.bufferDefinitions.push_back(parser.currentBuffer_);
            parser.parsingBuffer_ = false;
            parser.currentAccessSpec_ = nullptr;
            parser.capturingGPUAccess_ = false;
            parser.capturingCPUAccess_ = false;
        }
    };

    // ========================================================================
    // SAMPLERS PARSING
    // ========================================================================

    template<>
    struct ShaderParserPEGTL::action<grammar::samplers_keyword> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            parser.parsingSamplers_ = true;
        }
    };

    template<>
    struct ShaderParserPEGTL::action<grammar::variable_decl> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, ShaderParserPEGTL& parser) {
            if (parser.parsingStruct_ || parser.parsingBuffer_) {
                return; // Handled by struct_field
            }

            if (parser.parsingSamplers_) {
                InputVariable var;
                var.name = parser.capturedName_;
                var.isSampler = true;
                var.typeInfo.baseType = parser.capturedType_;
                var.typeInfo.isArray = parser.capturedIsArray_;
                var.typeInfo.arraySize = parser.capturedArraySize_;
                var.type = parser.capturedType_;
                if (parser.capturedIsArray_) {
                    var.type += "[" + std::to_string(parser.capturedArraySize_) + "]";
                }

                parser.data_.samplerVariables.push_back(var);

                parser.capturedType_.clear();
                parser.capturedName_.clear();
                parser.capturedIsArray_ = false;
                parser.capturedArraySize_ = 0;
            }
        }
    };

    // ========================================================================
    // STAGE PARSING
    // ========================================================================

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

    // ========================================================================
    // MAIN PARSE METHOD
    // ========================================================================

    ParsedShaderData ShaderParserPEGTL::Parse(const std::string& source) {
        // Reset state
        data_ = ParsedShaderData();
        currentStage_ = ShaderLib::Stage::Vertex;
        capturedType_.clear();
        capturedName_.clear();
        capturedIsArray_ = false;
        capturedArraySize_ = 0;
        parsingStruct_ = false;
        parsingBuffer_ = false;
        parsingSamplers_ = false;
        currentAccessSpec_ = nullptr;

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
