#include "pch.h"
#include "ShaderLexer.h"
#include <cctype>
#include <algorithm>

namespace Shader {

    ShaderLexer::ShaderLexer(const std::string& source)
        : source_(source), pos_(0), line_(1), column_(1), maxErrors_(10) {
    }

    std::vector<Token> ShaderLexer::Tokenize() {
        std::vector<Token> tokens;
        errors_.clear();

        while (pos_ < source_.length() && ShouldContinue()) {
            char current = CurrentChar();

            if (std::isspace(current)) {
                SkipWhitespace();
                continue;
            }

            if (current == '/' && PeekChar() == '/') {
                tokens.push_back(ReadComment());
                continue;
            }

            if (current == '/' && PeekChar() == '*') {
                tokens.push_back(ReadComment());
                continue;
            }

            if (current == '"') {
                tokens.push_back(ReadString());
                continue;
            }

            if (current == '#') {
                tokens.push_back(ReadDirective());
                continue;
            }

            if (IsAlpha(current) || current == '_') {
                tokens.push_back(ReadIdentifier());
                continue;
            }

            if (IsDigit(current)) {
                tokens.push_back(ReadNumber());
                continue;
            }

            // Rozpoznaj podstawowe tokeny strukturalne
            switch (current) {
            case '{':
                tokens.push_back({ TokenType::LeftBrace, "{", line_, column_, pos_ });
                break;
            case '}':
                tokens.push_back({ TokenType::RightBrace, "}", line_, column_, pos_ });
                break;
            case ';':
                tokens.push_back({ TokenType::Semicolon, ";", line_, column_, pos_ });
                break;
            case '(':
            case ')':
            case '[':
            case ']':
            case ',':
            case '.':
            case '=':
            case '+':
            case '-':
            case '*':
            case '/':
            case '<':
            case '>':
            case '!':
            case '&':
            case '|':
            case '^':
            case '~':
            case '%':
            case '?':
            case ':':
                // Znane tokeny GLSL - oznacz jako Unknown ale nie zgłaszaj błędu
                tokens.push_back({ TokenType::Unknown, std::string(1, current), line_, column_, pos_ });
                break;
            default:
                // Prawdziwie nieznane tokeny
                tokens.push_back({ TokenType::Unknown, std::string(1, current), line_, column_, pos_ });
                break;
            }

            Advance();
        }

        tokens.push_back({ TokenType::EndOfFile, "", line_, column_, pos_ });
        return tokens;
    }

    char ShaderLexer::CurrentChar() {
        if (pos_ >= source_.length()) return '\0';
        return source_[pos_];
    }

    char ShaderLexer::PeekChar(size_t offset) {
        size_t peekPos = pos_ + offset;
        if (peekPos >= source_.length()) return '\0';
        return source_[peekPos];
    }

    void ShaderLexer::Advance() {
        if (pos_ < source_.length()) {
            if (source_[pos_] == '\n') {
                line_++;
                column_ = 1;
            }
            else {
                column_++;
            }
            pos_++;
        }
    }

    void ShaderLexer::SkipWhitespace() {
        while (pos_ < source_.length() && std::isspace(CurrentChar())) {
            Advance();
        }
    }

    Token ShaderLexer::ReadIdentifier() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        while (pos_ < source_.length() && (IsAlphaNumeric(CurrentChar()) || CurrentChar() == '_')) {
            Advance();
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::Identifier, value, startLine, startColumn, start };
    }

    Token ShaderLexer::ReadDirective() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        while (pos_ < source_.length() && CurrentChar() != '\n' && CurrentChar() != '\r') {
            Advance();
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::Directive, value, startLine, startColumn, start };
    }

    Token ShaderLexer::ReadString() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        Advance(); // Skip opening quote

        while (pos_ < source_.length() && CurrentChar() != '"') {
            if (CurrentChar() == '\\') {
                Advance();
                if (pos_ < source_.length()) {
                    Advance();
                }
            }
            else {
                Advance();
            }
        }

        if (pos_ >= source_.length()) {
            ReportError("Unterminated string literal", startLine, startColumn, start);
            return { TokenType::String, source_.substr(start, pos_ - start), startLine, startColumn, start };
        }

        if (pos_ < source_.length()) {
            Advance(); // Skip closing quote
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::String, value, startLine, startColumn, start };
    }

    Token ShaderLexer::ReadNumber() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        bool hasDecimalPoint = false;

        while (pos_ < source_.length()) {
            char c = CurrentChar();
            if (IsDigit(c)) {
                Advance();
            }
            else if (c == '.' && !hasDecimalPoint) {
                hasDecimalPoint = true;
                Advance();
            }
            else {
                break;
            }
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::Number, value, startLine, startColumn, start };
    }

    Token ShaderLexer::ReadComment() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        if (CurrentChar() == '/' && PeekChar() == '/') {
            // Single-line comment
            while (pos_ < source_.length() && CurrentChar() != '\n') {
                Advance();
            }
        }
        else if (CurrentChar() == '/' && PeekChar() == '*') {
            // Multi-line comment
            Advance(); // Skip '/'
            Advance(); // Skip '*'

            bool commentClosed = false;
            while (pos_ < source_.length() - 1) {
                if (CurrentChar() == '*' && PeekChar() == '/') {
                    Advance(); // Skip '*'
                    Advance(); // Skip '/'
                    commentClosed = true;
                    break;
                }
                Advance();
            }

            if (!commentClosed) {
                ReportError("Unterminated multi-line comment", startLine, startColumn, start);
                // Also report to ShaderErrorManager
                auto& errorManager = ShaderErrorManager::Instance();
                errorManager.ReportLexerError("Unterminated multi-line comment", startLine, startColumn, start, "", "");
            }
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::Comment, value, startLine, startColumn, start };
    }

    bool ShaderLexer::IsAlpha(char c) {
        return std::isalpha(c) || c == '_';
    }

    bool ShaderLexer::IsAlphaNumeric(char c) {
        return std::isalnum(c) || c == '_';
    }

    bool ShaderLexer::IsDigit(char c) {
        return std::isdigit(c);
    }

    bool ShaderLexer::IsValidPosition(size_t pos) {
        return pos < source_.length();
    }

    void ShaderLexer::ReportError(const std::string& message) {
        ReportError(message, line_, column_, pos_);
    }

    void ShaderLexer::ReportError(const std::string& message, size_t line, size_t column, size_t position) {
        errors_.push_back({ message, line, column, position });
        RecoverFromError();
    }

    bool ShaderLexer::ShouldContinue() {
        return errors_.size() < maxErrors_;
    }

    void ShaderLexer::RecoverFromError() {
        // Skip to next safe position
        SkipToNextToken();
    }

    void ShaderLexer::SkipToNextLine() {
        while (pos_ < source_.length() && CurrentChar() != '\n') {
            Advance();
        }
        if (pos_ < source_.length()) {
            Advance(); // Skip newline
        }
    }

    void ShaderLexer::SkipToNextToken() {
        while (pos_ < source_.length()) {
            char c = CurrentChar();
            if (std::isspace(c) || c == ';' || c == '{' || c == '}') {
                break;
            }
            Advance();
        }
    }

} // namespace Shader