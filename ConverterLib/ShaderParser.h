#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <ShaderLib.h>

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

    // Token types for lexer
    enum class TokenType {
        Identifier,
        Directive,
        LeftBrace,
        RightBrace,
        Semicolon,
        Whitespace,
        Comment,
        String,
        Number,
        EndOfFile,
        Unknown
    };

    struct Token {
        TokenType type;
        std::string value;
        size_t line;
        size_t column;
        size_t position;
    };

    class ShaderLexer {
    public:
        ShaderLexer(const std::string& source);
        std::vector<Token> Tokenize();

    private:
        std::string source_;
        size_t pos_;
        size_t line_;
        size_t column_;

        char CurrentChar();
        char PeekChar(size_t offset = 1);
        void Advance();
        void SkipWhitespace();
        Token ReadIdentifier();
        Token ReadDirective();
        Token ReadString();
        Token ReadNumber();
        Token ReadComment();
        bool IsAlpha(char c);
        bool IsAlphaNumeric(char c);
        bool IsDigit(char c);
    };

    class ShaderParser {
    public:
        ShaderParser();
        ParsedShaderData Parse(const std::string& source);

    private:
        std::vector<Token> tokens_;
        size_t current_;
        std::string originalSource_; // Store the original source for stage extraction

        // Supported directives and their handlers
        std::unordered_map<std::string, std::function<void(const Token&)>> directiveHandlers_;

        // Parsing state
        ParsedShaderData result_;

        // Core parsing methods
        void InitializeDirectiveHandlers();
        void ParseTokens();
        Token CurrentToken();
        Token PeekToken(size_t offset = 1);
        void Advance();
        bool IsAtEnd();

        // Directive handlers
        void HandleVersionDirective(const Token& token);
        void HandleUseDirective(const Token& token);
        void HandleStageDirective(const Token& token);
        void HandleInputDataBlock();

        // Utility methods
        std::string ExtractDirectiveValue(const Token& token, const std::string& directive);
        std::vector<InputVariable> ParseInputDataFields();
        std::string ExtractStageCode(size_t startPos);
        bool IsValidContext(); // Check if we're not in comments or strings
    };

} // namespace Shader