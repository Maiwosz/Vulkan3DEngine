#pragma once
#include <string>
#include <vector>
#include <tao/pegtl.hpp>
#include <ShaderLib.h>

namespace Shader {

    struct InputVariable {
        std::string type;
        std::string name;
        bool isSampler;
    };

    struct ShaderStage {
        std::string code;
        ShaderLib::Stage stage;
    };

    struct ParsedShaderData {
        std::string versionLine;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;
        std::vector<InputVariable> inputVariables;
        std::vector<ShaderStage> stages;
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

        // Version directive: #version 450
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

        // Use directive: #use global_ubo
        struct use_name : pegtl::identifier {};
        struct use_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'u', 's', 'e'>,
            ws_plus,
            use_name
        > {
        };

        // InputData block
        struct type_name : pegtl::identifier {};
        struct var_name : pegtl::identifier {};

        struct variable_decl : pegtl::seq<
            type_name,
            ws_plus,
            var_name,
            ws_star,
            pegtl::one<';'>
        > {
        };

        struct input_data_block : pegtl::seq<
            pegtl::string<'I', 'n', 'p', 'u', 't', 'D', 'a', 't', 'a'>,
            ws_star,
            pegtl::one<'{'>,
            pegtl::star<pegtl::sor<variable_decl, ws>>,
            pegtl::one<'}'>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // Stage directive: #stage vertex
        struct stage_name : pegtl::identifier {};
        struct stage_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'s', 't', 'a', 'g', 'e'>,
            ws_plus,
            stage_name
        > {
        };

        // Stage code (everything until next #stage or EOF)
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
            input_data_block,
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

        // State for capturing content
        std::string capturedType_;
        std::string capturedName_;

        friend struct action<grammar::version_directive>;
        friend struct action<grammar::use_name>;
        friend struct action<grammar::type_name>;
        friend struct action<grammar::var_name>;
        friend struct action<grammar::variable_decl>;
        friend struct action<grammar::stage_name>;
        friend struct action<grammar::stage_code>;
    };

} // namespace Shader