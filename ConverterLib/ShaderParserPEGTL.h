#pragma once
#include <string>
#include <vector>
#include <variant>
#include <tao/pegtl.hpp>
#include <ShaderLib.h>

namespace Shader {

    // Forward declarations
    struct StructDefinition;
    struct ArrayDefinition;

    // Type może być prosty, struct lub array
    struct TypeInfo {
        std::string baseType;  // "float", "vec3", lub nazwa struktury
        bool isArray = false;
        uint32_t arraySize = 0;
        bool isStruct = false;
        std::shared_ptr<StructDefinition> structDef;
    };

    struct StructField {
        TypeInfo type;
        std::string name;
    };

    struct StructDefinition {
        std::string name;
        std::vector<StructField> fields;
    };

    struct InputVariable {
        std::string type;
        std::string name;
        bool isSampler;
        TypeInfo typeInfo;  // Rozszerzona informacja o typie
    };

    struct ShaderStage {
        std::string code;
        ShaderLib::Stage stage;
    };

    enum class ShaderDataStructType {
        Input,
        Output,
        InputOutput,
        Samplers
    };

    struct ParsedShaderData {
        std::string versionLine;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;

        std::vector<InputVariable> inputVariables;
        std::vector<InputVariable> outputVariables;
        std::vector<InputVariable> inputOutputVariables;
        std::vector<InputVariable> samplerVariables;

        // Definicje struktur zdefiniowanych w ShaderData
        std::vector<StructDefinition> structDefinitions;

        std::vector<ShaderStage> stages;

        bool HasInputData() const { return !inputVariables.empty(); }
        bool HasOutputData() const { return !outputVariables.empty(); }
        bool HasInputOutputData() const { return !inputOutputVariables.empty(); }
        bool HasSamplers() const { return !samplerVariables.empty(); }

        const StructDefinition* FindStruct(const std::string& name) const {
            for (const auto& s : structDefinitions) {
                if (s.name == name) return &s;
            }
            return nullptr;
        }
    };

    namespace grammar {
        namespace pegtl = tao::pegtl;

        // Whitespace and comments
        struct line_comment : pegtl::seq<pegtl::two<'/'>, pegtl::until<pegtl::eolf>> {};
        struct block_comment : pegtl::seq<pegtl::string<'/', '*'>, pegtl::until<pegtl::string<'*', '/'>>> {};
        struct comment : pegtl::sor<line_comment, block_comment> {};
        struct ws : pegtl::sor<pegtl::space, comment> {};
        struct ws_star : pegtl::star<ws> {};
        struct ws_plus : pegtl::plus<ws> {};

        // Identifiers
        struct identifier : pegtl::identifier {};

        // Version directive
        struct version_number : pegtl::plus<pegtl::digit> {};
        struct version_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'v', 'e', 'r', 's', 'i', 'o', 'n'>,
            ws_plus,
            version_number,
            pegtl::until<pegtl::eolf>
        > {
        };

        // Use directive
        struct use_name : pegtl::identifier {};
        struct use_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'u', 's', 'e'>,
            ws_plus,
            use_name
        > {
        };

        // Type name (może być identyfikatorem lub strukturą)
        struct type_name : pegtl::identifier {};

        // Variable name
        struct var_name : pegtl::identifier {};

        // Array syntax: [size] after variable name
        struct array_size : pegtl::plus<pegtl::digit> {};
        struct array_bracket : pegtl::seq<
            pegtl::one<'['>,
            ws_star,
            array_size,
            ws_star,
            pegtl::one<']'>
        > {
        };

        // Struct definition wewnątrz ShaderData
        struct struct_keyword : pegtl::string<'s', 't', 'r', 'u', 'c', 't'> {};
        struct struct_name : pegtl::identifier {};

        // Struct field: type name[size];
        struct struct_field : pegtl::seq<
            type_name,
            ws_plus,
            var_name,
            ws_star,
            pegtl::opt<array_bracket>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        struct struct_body : pegtl::seq<
            pegtl::one<'{'>,
            ws_star,
            pegtl::star<pegtl::sor<struct_field, ws>>,
            pegtl::one<'}'>
        > {
        };

        struct struct_definition : pegtl::seq<
            struct_keyword,
            ws_plus,
            struct_name,
            ws_star,
            struct_body,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // Variable declaration: type name[size];
        struct variable_decl : pegtl::seq<
            type_name,
            ws_plus,
            var_name,
            ws_star,
            pegtl::opt<array_bracket>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        struct shader_data_keyword : pegtl::string<'S', 'h', 'a', 'd', 'e', 'r', 'D', 'a', 't', 'a'> {};
        struct input_data_keyword : pegtl::string<'I', 'n', 'p', 'u', 't', 'D', 'a', 't', 'a'> {};
        struct output_data_keyword : pegtl::string<'O', 'u', 't', 'p', 'u', 't', 'D', 'a', 't', 'a'> {};
        struct input_output_data_keyword : pegtl::string<'I', 'n', 'p', 'u', 't', 'O', 'u', 't', 'p', 'u', 't', 'D', 'a', 't', 'a'> {};
        struct samplers_keyword : pegtl::string<'S', 'a', 'm', 'p', 'l', 'e', 'r', 's'> {};

        struct data_struct_type : pegtl::sor<
            input_data_keyword,
            output_data_keyword,
            input_output_data_keyword,
            samplers_keyword
        > {
        };

        struct data_struct_block : pegtl::seq<
            data_struct_type,
            ws_star,
            pegtl::one<'{'>,
            pegtl::star<pegtl::sor<variable_decl, ws>>,
            pegtl::one<'}'>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // ShaderData może zawierać struktury i bloki danych
        struct shader_data_content : pegtl::sor<
            struct_definition,
            data_struct_block,
            ws
        > {
        };

        struct shader_data_block : pegtl::seq<
            shader_data_keyword,
            ws_star,
            pegtl::one<'{'>,
            pegtl::star<shader_data_content>,
            pegtl::one<'}'>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // Stage directive
        struct stage_name : pegtl::identifier {};
        struct stage_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'s', 't', 'a', 'g', 'e'>,
            ws_plus,
            stage_name
        > {
        };

        // Stage code
        struct stage_code : pegtl::until<
            pegtl::sor<
            pegtl::at<stage_directive>,
            pegtl::eof
            >
        > {
        };

        // Complete stage block
        struct stage_block : pegtl::seq<stage_directive, stage_code> {};

        // Pre-stage content
        struct pre_stage : pegtl::star<
            pegtl::sor<
            version_directive,
            use_directive,
            shader_data_block,
            pegtl::seq<pegtl::not_at<stage_directive>, pegtl::any>
            >
        > {
        };

        // Main grammar
        struct grammar : pegtl::must<
            pre_stage,
            pegtl::plus<stage_block>,
            pegtl::eof
        > {
        };

    } // namespace grammar

    class ShaderParserPEGTL {
    public:
        ParsedShaderData Parse(const std::string& source);

    private:
        template<typename Rule>
        struct action : tao::pegtl::nothing<Rule> {};

        ParsedShaderData data_;
        ShaderLib::Stage currentStage_ = ShaderLib::Stage::Vertex;
        ShaderDataStructType currentStructType_;

        // State for capturing
        std::string capturedType_;
        std::string capturedName_;
        uint32_t capturedArraySize_ = 0;
        bool capturedIsArray_ = false;

        // Struct parsing state
        StructDefinition currentStruct_;
        bool parsingStruct_ = false;

        friend struct action<grammar::version_directive>;
        friend struct action<grammar::use_name>;
        friend struct action<grammar::type_name>;
        friend struct action<grammar::array_size>;
        friend struct action<grammar::var_name>;
        friend struct action<grammar::variable_decl>;
        friend struct action<grammar::struct_name>;
        friend struct action<grammar::struct_field>;
        friend struct action<grammar::struct_definition>;
        friend struct action<grammar::stage_name>;
        friend struct action<grammar::stage_code>;
        friend struct action<grammar::input_data_keyword>;
        friend struct action<grammar::output_data_keyword>;
        friend struct action<grammar::input_output_data_keyword>;
        friend struct action<grammar::samplers_keyword>;
    };

} // namespace Shader