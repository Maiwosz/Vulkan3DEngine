#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <ShaderLib.h>
#include "ShaderLexer.h"

using namespace ShaderLib;

namespace Shader {

    struct InputVariable {
        std::string type;
        std::string name;
        bool isSampler;
    };

    struct ShaderStage {
        std::string code;
        Stage stage;
    };

    struct ParsedShaderData {
        std::string versionLine;
        std::vector<InputVariable> inputVariables;
        bool usesGlobalUBO;
        bool usesObjectUBO;
        std::vector<ShaderStage> stages;
    };

    // Error handling for parser
    struct ParseError {
        std::string message;
        size_t line;
        size_t column;
        size_t position;
        std::string context;
    };

    // Immutable parse context - now with pointers instead of references
    struct ParseContext {
        const std::vector<Token>* tokens;
        const std::string* originalSource;
        size_t currentPosition;
        std::vector<ParseError> errors;
        size_t maxErrors;

        ParseContext(const std::vector<Token>& tokens, const std::string& source, size_t maxErrors = 20)
            : tokens(&tokens), originalSource(&source), currentPosition(0), maxErrors(maxErrors) {
        }

        // Now copy assignment works fine since we're copying pointers
        ParseContext(const ParseContext&) = default;
        ParseContext& operator=(const ParseContext&) = default;
    };

    class ShaderParser {
    public:
        ShaderParser();

        ParsedShaderData Parse(const std::string& source);

        // Error handling
        bool HasErrors() const { return !errors_.empty(); }
        const std::vector<ParseError>& GetErrors() const { return errors_; }

        // Configuration
        void SetMaxErrors(size_t maxErrors) { maxErrors_ = maxErrors; }

    private:
        std::vector<ParseError> errors_;
        size_t maxErrors_;

        // Core parsing methods
        ParsedShaderData ParseWithContext(ParseContext& context);
        ParseContext ParseTokens(ParseContext context, ParsedShaderData& result);

        // Token navigation
        Token CurrentToken(const ParseContext& context);
        Token PeekToken(const ParseContext& context, size_t offset = 1);
        ParseContext Advance(ParseContext context);
        bool IsAtEnd(const ParseContext& context);

        // Specific parsing methods
        ParseContext HandleDirective(ParseContext context, ParsedShaderData& result);
        ParseContext HandleStageDirective(ParseContext context, ParsedShaderData& result);
        ParseContext HandleInputDataBlock(ParseContext context, ParsedShaderData& result);
        std::vector<InputVariable> ParseInputDataFields(ParseContext& context);
        bool IsValidContext(const ParseContext& context);

        // Error handling and recovery
        void ReportError(const std::string& message, const ParseContext& context);
        void ReportError(const std::string& message, const Token& token);
        bool ShouldContinue(const ParseContext& context);
        ParseContext RecoverFromError(ParseContext context);
        ParseContext SkipToNextStatement(ParseContext context);
        ParseContext SkipToNextBlock(ParseContext context);

        // Validation
        bool ValidateTokenIndex(const ParseContext& context, size_t index);
        bool ValidateShaderStages(const ParsedShaderData& data);
    };

} // namespace Shader