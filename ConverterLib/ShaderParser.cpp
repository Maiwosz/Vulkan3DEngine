#include "pch.h"
#include "ShaderParser.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <TypeConversions.h>

namespace Shader {

    // ShaderLexer Implementation
    ShaderLexer::ShaderLexer(const std::string& source)
        : source_(source), pos_(0), line_(1), column_(1) {
    }

    std::vector<Token> ShaderLexer::Tokenize() {
        std::vector<Token> tokens;

        while (pos_ < source_.length()) {
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

            // Single character tokens
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
            default:
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

        // Read until end of line
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
                Advance(); // Skip escape character
                if (pos_ < source_.length()) {
                    Advance(); // Skip escaped character
                }
            }
            else {
                Advance();
            }
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

        while (pos_ < source_.length() && (IsDigit(CurrentChar()) || CurrentChar() == '.')) {
            Advance();
        }

        std::string value = source_.substr(start, pos_ - start);
        return { TokenType::Number, value, startLine, startColumn, start };
    }

    Token ShaderLexer::ReadComment() {
        size_t start = pos_;
        size_t startLine = line_;
        size_t startColumn = column_;

        if (CurrentChar() == '/' && PeekChar() == '/') {
            // Single line comment
            while (pos_ < source_.length() && CurrentChar() != '\n') {
                Advance();
            }
        }
        else if (CurrentChar() == '/' && PeekChar() == '*') {
            // Multi-line comment
            Advance(); // Skip '/'
            Advance(); // Skip '*'

            while (pos_ < source_.length() - 1) {
                if (CurrentChar() == '*' && PeekChar() == '/') {
                    Advance(); // Skip '*'
                    Advance(); // Skip '/'
                    break;
                }
                Advance();
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

    // ShaderParser Implementation
    ShaderParser::ShaderParser() : current_(0) {
        InitializeDirectiveHandlers();
    }

    void ShaderParser::InitializeDirectiveHandlers() {
        directiveHandlers_["#version"] = [this](const Token& token) {
            HandleVersionDirective(token);
            };

        directiveHandlers_["#use"] = [this](const Token& token) {
            HandleUseDirective(token);
            };

        directiveHandlers_["#stage"] = [this](const Token& token) {
            HandleStageDirective(token);
            };
    }

    ParsedShaderData ShaderParser::Parse(const std::string& source) {
        originalSource_ = source; // Store the original source

        ShaderLexer lexer(source);
        tokens_ = lexer.Tokenize();
        current_ = 0;
        result_ = ParsedShaderData{};

        // Set default version if not specified
        result_.versionLine = "#version 450";

        ParseTokens();

        return result_;
    }

    void ShaderParser::ParseTokens() {
        while (!IsAtEnd()) {
            Token token = CurrentToken();

            if (token.type == TokenType::Directive) {
                // Extract directive name
                std::string directive = token.value;
                size_t spacePos = directive.find(' ');
                if (spacePos != std::string::npos) {
                    std::string directiveName = directive.substr(0, spacePos);

                    auto it = directiveHandlers_.find(directiveName);
                    if (it != directiveHandlers_.end()) {
                        it->second(token);
                    }
                }
            }
            else if (token.type == TokenType::Identifier && token.value == "InputData") {
                HandleInputDataBlock();
            }

            Advance();
        }
    }

    void ShaderParser::HandleVersionDirective(const Token& token) {
        result_.versionLine = token.value;
    }

    void ShaderParser::HandleUseDirective(const Token& token) {
        std::string value = ExtractDirectiveValue(token, "#use");

        if (value == "global_ubo") {
            result_.usesGlobalUBO = true;
        }
        else if (value == "object_ubo") {
            result_.usesObjectUBO = true;
        }
    }

    void ShaderParser::HandleStageDirective(const Token& token) {
        std::string stageName = ExtractDirectiveValue(token, "#stage");

        if (!stageName.empty()) {
            Stage stage = ShaderLib::TypeConversion::StringToStage(stageName);

            // Find the start of the actual stage code (skip the directive line)
            size_t codeStart = token.position + token.value.length();

            // Skip any whitespace and newlines after the directive
            while (codeStart < originalSource_.length() &&
                std::isspace(originalSource_[codeStart])) {
                codeStart++;
            }

            std::string code = ExtractStageCode(codeStart);
            result_.stages.push_back({ code, stage });
        }
    }

    void ShaderParser::HandleInputDataBlock() {
        // Look for opening brace
        while (!IsAtEnd() && CurrentToken().type != TokenType::LeftBrace) {
            Advance();
        }

        if (IsAtEnd()) return;

        result_.inputVariables = ParseInputDataFields();
    }

    std::string ShaderParser::ExtractDirectiveValue(const Token& token, const std::string& directive) {
        std::string value = token.value;

        if (value.length() > directive.length() + 1) {
            return value.substr(directive.length() + 1); // +1 for space
        }

        return "";
    }

    std::vector<InputVariable> ShaderParser::ParseInputDataFields() {
        std::vector<InputVariable> variables;

        Advance(); // Skip opening brace

        while (!IsAtEnd() && CurrentToken().type != TokenType::RightBrace) {
            Token token = CurrentToken();

            if (token.type == TokenType::Identifier) {
                std::string type = token.value;
                Advance();

                if (!IsAtEnd() && CurrentToken().type == TokenType::Identifier) {
                    std::string name = CurrentToken().value;
                    bool isSampler = type.find("sampler") != std::string::npos;

                    variables.push_back({ type, name, isSampler });
                }
            }

            Advance();
        }

        return variables;
    }

    std::string ShaderParser::ExtractStageCode(size_t startPos) {
        // Find the next #stage directive or end of file
        size_t endPos = originalSource_.length(); // Default to end of file

        for (size_t i = current_ + 1; i < tokens_.size(); ++i) {
            if (tokens_[i].type == TokenType::Directive &&
                tokens_[i].value.find("#stage") == 0) {
                endPos = tokens_[i].position;
                break;
            }
        }

        // Extract the original source code directly from the source string
        // This preserves all original spacing and formatting
        if (startPos < originalSource_.length() && endPos <= originalSource_.length()) {
            std::string stageCode = originalSource_.substr(startPos, endPos - startPos);

            // Remove any trailing whitespace and newlines
            while (!stageCode.empty() && std::isspace(stageCode.back())) {
                stageCode.pop_back();
            }

            return stageCode;
        }

        return "";
    }

    Token ShaderParser::CurrentToken() {
        if (current_ >= tokens_.size()) {
            return tokens_.back(); // Return EOF token
        }
        return tokens_[current_];
    }

    Token ShaderParser::PeekToken(size_t offset) {
        size_t peekPos = current_ + offset;
        if (peekPos >= tokens_.size()) {
            return tokens_.back(); // Return EOF token
        }
        return tokens_[peekPos];
    }

    void ShaderParser::Advance() {
        if (current_ < tokens_.size() - 1) {
            current_++;
        }
    }

    bool ShaderParser::IsAtEnd() {
        return current_ >= tokens_.size() - 1 ||
            CurrentToken().type == TokenType::EndOfFile;
    }

    bool ShaderParser::IsValidContext() {
        // Check if current token is not in a comment or string
        Token token = CurrentToken();
        return token.type != TokenType::Comment && token.type != TokenType::String;
    }

} // namespace Shader