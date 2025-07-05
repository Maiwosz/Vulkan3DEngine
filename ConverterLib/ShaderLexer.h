#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ShaderErrorManager.h"

namespace Shader {

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

    // Error handling for lexer
    struct LexerError {
        std::string message;
        size_t line;
        size_t column;
        size_t position;
    };

    class ShaderLexer {
    public:
        explicit ShaderLexer(const std::string& source);

        // Main tokenization method
        std::vector<Token> Tokenize();

        // Error handling
        bool HasErrors() const { return !errors_.empty(); }
        const std::vector<LexerError>& GetErrors() const { return errors_; }

        // Configuration
        void SetMaxErrors(size_t maxErrors) { maxErrors_ = maxErrors; }

    private:
        std::string source_;
        size_t pos_;
        size_t line_;
        size_t column_;
        std::vector<LexerError> errors_;
        size_t maxErrors_;

        // Character navigation
        char CurrentChar();
        char PeekChar(size_t offset = 1);
        void Advance();
        void SkipWhitespace();

        // Token reading with error handling
        Token ReadIdentifier();
        Token ReadDirective();
        Token ReadString();
        Token ReadNumber();
        Token ReadComment();

        // Utility methods
        bool IsAlpha(char c);
        bool IsAlphaNumeric(char c);
        bool IsDigit(char c);
        bool IsValidPosition(size_t pos);

        // Error handling
        void ReportError(const std::string& message);
        void ReportError(const std::string& message, size_t line, size_t column, size_t position);
        bool ShouldContinue();

        // Recovery methods
        void RecoverFromError();
        void SkipToNextLine();
        void SkipToNextToken();
    };

} // namespace Shader