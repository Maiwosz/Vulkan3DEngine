#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <tao/pegtl.hpp>
#include <ShaderLib.h>
#include <BufferAccessPatterns.h>

namespace Shader {

    // Forward declarations
    struct StructDefinition;
    struct ArrayDefinition;

    // Type może być prosty, struct lub array
    struct TypeInfo {
        std::string baseType;
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

    // Definicja wzorca dostępu procesora w parserze
    struct ProcessorAccessSpec {
        std::optional<ShaderLib::AccessFrequency> frequency;
        std::optional<ShaderLib::AccessOperation> operation;
        std::optional<ShaderLib::AccessSize> size;

        bool IsEmpty() const {
            return !frequency.has_value() &&
                !operation.has_value() &&
                !size.has_value();
        }
    };

    // Definicja bufora z typem i wzorcami dostępu
    struct BufferDefinition {
        std::string name;
        ShaderLib::BufferType bufferType;
        ProcessorAccessSpec gpuAccess;
        ProcessorAccessSpec cpuAccess;
        std::vector<StructField> fields;

        // Helper do tworzenia BufferAccessPatterns z wartościami domyślnymi
        ShaderLib::BufferAccessPatterns CreateAccessPatterns() const;
    };

    struct InputVariable {
        std::string type;
        std::string name;
        bool isSampler;
        TypeInfo typeInfo;
    };

    struct ShaderStage {
        std::string code;
        ShaderLib::Stage stage;
    };

    struct ParsedShaderData {
        std::string versionLine;
        bool usesGlobalUBO = false;
        bool usesObjectUBO = false;

        // Lista buforów zamiast predefiniowanych zmiennych
        std::vector<BufferDefinition> bufferDefinitions;
        std::vector<InputVariable> samplerVariables;

        // Definicje struktur zdefiniowanych w ShaderData
        std::vector<StructDefinition> structDefinitions;

        std::vector<ShaderStage> stages;

        bool HasSamplers() const { return !samplerVariables.empty(); }

        const StructDefinition* FindStruct(const std::string& name) const {
            for (const auto& s : structDefinitions) {
                if (s.name == name) return &s;
            }
            return nullptr;
        }

        const BufferDefinition* FindBuffer(const std::string& name) const {
            for (const auto& b : bufferDefinitions) {
                if (b.name == name) return &b;
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

        // Type name
        struct type_name : pegtl::identifier {};
        struct var_name : pegtl::identifier {};

        // Array syntax
        struct array_size : pegtl::plus<pegtl::digit> {};
        struct array_bracket : pegtl::seq<
            pegtl::one<'['>,
            ws_star,
            array_size,
            ws_star,
            pegtl::one<']'>
        > {
        };

        // Struct definition (standalone)
        struct struct_keyword : pegtl::string<'s', 't', 'r', 'u', 'c', 't'> {};
        struct struct_name : pegtl::identifier {};

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

        // =====================================================================
        // Access Pattern Grammar
        // =====================================================================

        // Access Frequency enums
        struct access_freq_never : pegtl::string<'N', 'e', 'v', 'e', 'r'> {};
        struct access_freq_onetime : pegtl::string<'O', 'n', 'e', 'T', 'i', 'm', 'e'> {};
        struct access_freq_everyfew : pegtl::string<'E', 'v', 'e', 'r', 'y', 'F', 'e', 'w', 'F', 'r', 'a', 'm', 'e', 's'> {};
        struct access_freq_onceperframe : pegtl::string<'O', 'n', 'c', 'e', 'P', 'e', 'r', 'F', 'r', 'a', 'm', 'e'> {};
        struct access_freq_multipleperframe : pegtl::string<'M', 'u', 'l', 't', 'i', 'p', 'l', 'e', 'P', 'e', 'r', 'F', 'r', 'a', 'm', 'e'> {};
        struct access_freq_continuous : pegtl::string<'C', 'o', 'n', 't', 'i', 'n', 'u', 'o', 'u', 's'> {};

        struct access_frequency : pegtl::sor<
            access_freq_continuous,
            access_freq_multipleperframe,
            access_freq_onceperframe,
            access_freq_everyfew,
            access_freq_onetime,
            access_freq_never
        > {
        };

        // Access Operation enums
        struct access_op_none : pegtl::string<'N', 'o', 'n', 'e'> {};
        struct access_op_readonly : pegtl::string<'R', 'e', 'a', 'd', 'O', 'n', 'l', 'y'> {};
        struct access_op_writeonly : pegtl::string<'W', 'r', 'i', 't', 'e', 'O', 'n', 'l', 'y'> {};
        struct access_op_readwrite : pegtl::string<'R', 'e', 'a', 'd', 'W', 'r', 'i', 't', 'e'> {};

        struct access_operation : pegtl::sor<
            access_op_readwrite,
            access_op_writeonly,
            access_op_readonly,
            access_op_none
        > {
        };

        // Access Size enums
        struct access_size_none : pegtl::string<'N', 'o', 'n', 'e'> {};
        struct access_size_tiny : pegtl::string<'T', 'i', 'n', 'y'> {};
        struct access_size_small : pegtl::string<'S', 'm', 'a', 'l', 'l'> {};
        struct access_size_medium : pegtl::string<'M', 'e', 'd', 'i', 'u', 'm'> {};
        struct access_size_large : pegtl::string<'L', 'a', 'r', 'g', 'e'> {};
        struct access_size_verylarge : pegtl::string<'V', 'e', 'r', 'y', 'L', 'a', 'r', 'g', 'e'> {};
        struct access_size_massive : pegtl::string<'M', 'a', 's', 's', 'i', 'v', 'e'> {};

        struct access_size : pegtl::sor<
            access_size_verylarge,
            access_size_massive,
            access_size_medium,
            access_size_small,
            access_size_large,
            access_size_tiny,
            access_size_none
        > {
        };

        // Single access property (dowolna kolejność)
        struct access_property : pegtl::sor<
            access_frequency,
            access_operation,
            access_size
        > {
        };

        // Lista właściwości oddzielonych przecinkami
        struct access_properties : pegtl::seq<
            access_property,
            pegtl::star<
            pegtl::seq<
            ws_star,
            pegtl::one<','>,
            ws_star,
            access_property
            >
            >
        > {
        };

        // Procesor access spec: gpu{properties} lub cpu{properties}
        struct processor_keyword_gpu : pegtl::string<'g', 'p', 'u'> {};
        struct processor_keyword_cpu : pegtl::string<'c', 'p', 'u'> {};

        struct processor_keyword : pegtl::sor<
            processor_keyword_gpu,
            processor_keyword_cpu
        > {
        };

        struct processor_access_spec : pegtl::seq<
            processor_keyword,
            ws_star,
            pegtl::one<'{'>,
            ws_star,
            pegtl::opt<access_properties>,
            ws_star,
            pegtl::one<'}'>
        > {
        };

        // Lista procesor specs oddzielonych przecinkami
        struct access_pattern_list : pegtl::seq<
            processor_access_spec,
            pegtl::star<
            pegtl::seq<
            ws_star,
            pegtl::one<','>,
            ws_star,
            processor_access_spec
            >
            >
        > {
        };

        // Buffer type: uniform lub storage
        struct buffer_type_uniform : pegtl::string<'u', 'n', 'i', 'f', 'o', 'r', 'm'> {};
        struct buffer_type_storage : pegtl::string<'s', 't', 'o', 'r', 'a', 'g', 'e'> {};

        struct buffer_type : pegtl::sor<
            buffer_type_uniform,
            buffer_type_storage
        > {
        };

        // =====================================================================
        // Buffer Definition: Name:type(access_patterns){fields};
        // =====================================================================

        struct buffer_name : pegtl::identifier {};

        struct buffer_header : pegtl::seq<
            buffer_name,
            ws_star,
            pegtl::one<':'>,
            ws_star,
            buffer_type,
            ws_star,
            pegtl::one<'('>,
            ws_star,
            access_pattern_list,
            ws_star,
            pegtl::one<')'>
        > {
        };

        struct buffer_body : pegtl::seq<
            pegtl::one<'{'>,
            ws_star,
            pegtl::star<pegtl::sor<struct_field, ws>>,
            pegtl::one<'}'>
        > {
        };

        struct buffer_definition : pegtl::seq<
            buffer_header,
            ws_star,
            buffer_body,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // =====================================================================
        // Samplers block
        // =====================================================================

        struct samplers_keyword : pegtl::string<'S', 'a', 'm', 'p', 'l', 'e', 'r', 's'> {};

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

        struct samplers_block : pegtl::seq<
            samplers_keyword,
            ws_star,
            pegtl::one<'{'>,
            pegtl::star<pegtl::sor<variable_decl, ws>>,
            pegtl::one<'}'>,
            ws_star,
            pegtl::one<';'>
        > {
        };

        // =====================================================================
        // ShaderData block
        // =====================================================================

        struct shader_data_keyword : pegtl::string<'S', 'h', 'a', 'd', 'e', 'r', 'D', 'a', 't', 'a'> {};

        struct shader_data_content : pegtl::sor<
            struct_definition,
            samplers_block,
            buffer_definition,
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

        // =====================================================================
        // Stage directives
        // =====================================================================

        struct stage_name : pegtl::identifier {};
        struct stage_directive : pegtl::seq<
            pegtl::one<'#'>,
            ws_star,
            pegtl::string<'s', 't', 'a', 'g', 'e'>,
            ws_plus,
            stage_name
        > {
        };

        struct stage_code : pegtl::until<
            pegtl::sor<
            pegtl::at<stage_directive>,
            pegtl::eof
            >
        > {
        };

        struct stage_block : pegtl::seq<stage_directive, stage_code> {};

        // =====================================================================
        // Main grammar
        // =====================================================================

        struct pre_stage : pegtl::star<
            pegtl::sor<
            version_directive,
            use_directive,
            shader_data_block,
            pegtl::seq<pegtl::not_at<stage_directive>, pegtl::any>
            >
        > {
        };

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

        // State for capturing
        std::string capturedType_;
        std::string capturedName_;
        uint32_t capturedArraySize_ = 0;
        bool capturedIsArray_ = false;

        // Struct parsing state
        StructDefinition currentStruct_;
        bool parsingStruct_ = false;

        // Buffer parsing state
        BufferDefinition currentBuffer_;
        bool parsingBuffer_ = false;
        bool capturingGPUAccess_ = false;
        bool capturingCPUAccess_ = false;
        ProcessorAccessSpec* currentAccessSpec_ = nullptr;

        // Samplers parsing
        bool parsingSamplers_ = false;

        // Friends dla wszystkich akcji
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
        friend struct action<grammar::samplers_keyword>;

        // Buffer parsing friends
        friend struct action<grammar::buffer_name>;
        friend struct action<grammar::buffer_type>;
        friend struct action<grammar::processor_keyword_gpu>;
        friend struct action<grammar::processor_keyword_cpu>;
        friend struct action<grammar::access_frequency>;
        friend struct action<grammar::access_operation>;
        friend struct action<grammar::access_size>;
        friend struct action<grammar::buffer_definition>;
    };

} // namespace Shader
