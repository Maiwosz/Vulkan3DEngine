#include "pch.h"
#include "ShaderParser.h"
#include "ParserDictionary.h"
#include "ShaderLexer.h"
#include "ShaderErrorManager.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace Shader {

    ShaderParser::ShaderParser() : maxErrors_(20) {
    }

    ParsedShaderData ShaderParser::Parse(const std::string& source) {
        errors_.clear();

        // Get reference to error manager
        auto& errorManager = ShaderErrorManager::Instance();
        errorManager.Clear();

        if (source.empty()) {
            errorManager.ReportParserError("Empty shader source", 0, 0, 0, "", "");
            return ParsedShaderData{};
        }

        try {
            ShaderLexer lexer(source);
            std::vector<Token> tokens = lexer.Tokenize();

            // Report lexer errors to error manager
            for (const auto& lexerError : lexer.GetErrors()) {
                errorManager.ReportLexerError(lexerError.message, lexerError.line,
                    lexerError.column, lexerError.position, "", "");
            }

            // Copy lexer errors to parser errors for backward compatibility
            for (const auto& lexerError : lexer.GetErrors()) {
                errors_.push_back({
                    lexerError.message,
                    lexerError.line,
                    lexerError.column,
                    lexerError.position,
                    "Lexer"
                    });
            }

            ParseContext context(tokens, source, maxErrors_);
            ParsedShaderData result = ParseWithContext(context);

            // Report parser errors to error manager
            for (const auto& error : errors_) {
                errorManager.ReportParserError(error.message, error.line,
                    error.column, error.position, error.context, "");
            }

            // Don't fail completely on errors - return partial results
            if (errorManager.HasFatalErrors()) {
                std::cout << "Fatal parser errors found:\n" << errorManager.FormatNonInfoErrors() << std::endl;
                return ParsedShaderData{}; // Return empty data only on fatal errors
            }
            else if (errorManager.HasNonWarningErrors()) {
                std::cout << "Parser errors found (continuing with partial results):\n" << errorManager.FormatNonInfoErrors() << std::endl;
            }

            return result;
        }
        catch (const std::exception& e) {
            errorManager.ReportParserError("Parser exception: " + std::string(e.what()), 0, 0, 0, "", "");
            return ParsedShaderData{};
        }
    }

    ParsedShaderData ShaderParser::ParseWithContext(ParseContext& context) {
        ParsedShaderData result;
        result.versionLine = "#version 450"; // Default version
        result.usesGlobalUBO = false;
        result.usesObjectUBO = false;

        try {
            context = ParseTokens(context, result); // Pass result by reference

            // Copy context errors to parser errors
            for (const auto& error : context.errors) {
                errors_.push_back(error);
            }

            // Check for actual errors (not info messages)
            auto& errorManager = ShaderErrorManager::Instance();
            if (errorManager.HasFatalErrors()) {
                std::cout << "Fatal parser errors found:\n" << errorManager.FormatNonInfoErrors() << std::endl;
                return ParsedShaderData{}; // Return empty data only on fatal errors
            }
            else if (errorManager.HasNonWarningErrors()) {
                std::cout << "Parser errors found (continuing with partial results):\n" << errorManager.FormatNonInfoErrors() << std::endl;
            }

            // Don't return empty data if we have some valid content
            if (result.stages.empty() && result.inputVariables.empty()) {
                // Only report as error if we actually expected content
                if (context.tokens->size() > 1) { // More than just EOF token
                    errorManager.ReportParserError("No shader stages or input variables found", 0, 0, 0, "", "");
                }
            }

            // Even if we have errors, return what we parsed successfully
            return result;
        }
        catch (const std::exception& e) {
            ReportError("Exception in ParseWithContext: " + std::string(e.what()), context);
            return result; // Return partial results even on exception
        }
    }

    ParseContext ShaderParser::ParseTokens(ParseContext context, ParsedShaderData& result) {
        ParserDictionary dictionary;

        while (!IsAtEnd(context) && ShouldContinue(context)) {
            Token token = CurrentToken(context);

            try {
                if (token.type == TokenType::Directive) {
                    if (token.value.substr(0, 6) == "#stage") {
                        // Handle stage directive and skip its content
                        context = HandleStageDirective(context, result);
                    }
                    else {
                        context = HandleDirective(context, result);
                    }
                }
                else if (token.type == TokenType::Identifier) {
                    if (token.value == "InputData") {
                        context = HandleInputDataBlock(context, result);
                    }
                    else {
                        // Check if this is a GLSL type followed by a name (potential variable declaration)
                        std::string nextTokenValue = "";
                        if (context.currentPosition + 1 < context.tokens->size()) {
                            Token nextToken = (*context.tokens)[context.currentPosition + 1];
                            if (nextToken.type == TokenType::Identifier) {
                                nextTokenValue = nextToken.value;
                            }
                        }

                        // For now, just skip unknown identifiers but don't report as error
                        context = Advance(context);
                    }
                }
                else if (token.type == TokenType::Comment) {
                    // Skip comments
                    context = Advance(context);
                }
                else if (token.type == TokenType::Unknown) {
                    // Only report as error if it's truly unexpected
                    if (token.value != "{" && token.value != "}" && token.value != ";" &&
                        token.value != "(" && token.value != ")" && token.value != "," &&
                        token.value != "=" && token.value != "." && token.value != "[" && token.value != "]") {
                        ReportError("Unknown token: " + token.value, context);
                    }
                    context = Advance(context);
                }
                else {
                    // Skip other tokens (numbers, strings, etc.)
                    context = Advance(context);
                }
            }
            catch (const std::exception& e) {
                ReportError("Parser exception: " + std::string(e.what()), context);
                context = RecoverFromError(context);
            }
        }

        return context;
    }

    ParseContext ShaderParser::HandleDirective(ParseContext context, ParsedShaderData& result) {
        Token token = CurrentToken(context);
        ParserDictionary dictionary;

        std::string directive = token.value;
        size_t spacePos = directive.find(' ');

        if (spacePos != std::string::npos) {
            std::string directiveName = directive.substr(0, spacePos);

            if (dictionary.IsDirectiveSupported(directiveName)) {
                auto handler = dictionary.GetDirectiveHandler(directiveName);
                if (handler) {
                    try {
                        handler(token, result, *context.originalSource);
                    }
                    catch (const std::exception& e) {
                        ReportError("Error handling directive " + directiveName + ": " + e.what(), context);
                    }
                }
            }
            else {
                ReportError("Unsupported directive: " + directiveName, context);
            }
        }
        else {
            ReportError("Invalid directive format: " + directive, context);
        }

        return Advance(context);
    }

    ParseContext ShaderParser::HandleStageDirective(ParseContext context, ParsedShaderData& result) {
        Token token = CurrentToken(context);
        ParserDictionary dictionary;

        std::string directive = token.value;
        size_t spacePos = directive.find(' ');

        if (spacePos != std::string::npos) {
            std::string directiveName = directive.substr(0, spacePos);

            if (dictionary.IsDirectiveSupported(directiveName)) {
                auto handler = dictionary.GetDirectiveHandler(directiveName);
                if (handler) {
                    try {
                        handler(token, result, *context.originalSource);
                    }
                    catch (const std::exception& e) {
                        ReportError("Error handling directive " + directiveName + ": " + e.what(), context);
                    }
                }
            }
        }

        context = Advance(context); // Skip the #stage directive

        // Skip all tokens until next #stage directive or end of file
        while (!IsAtEnd(context)) {
            Token nextToken = CurrentToken(context);

            if (nextToken.type == TokenType::Directive &&
                nextToken.value.substr(0, 6) == "#stage") {
                break; // Found next #stage directive
            }

            context = Advance(context);
        }

        return context;
    }

    ParseContext ShaderParser::HandleInputDataBlock(ParseContext context, ParsedShaderData& result) {
        auto& errorManager = ShaderErrorManager::Instance();

        // Skip to opening brace
        while (!IsAtEnd(context) && CurrentToken(context).type != TokenType::LeftBrace) {
            context = Advance(context);
        }

        if (IsAtEnd(context)) {
            ReportError("Expected '{' after InputData", context);
            return context;
        }

        context = Advance(context); // Skip opening brace

        int braceCount = 1;
        bool foundClosingBrace = false;

        while (!IsAtEnd(context) && braceCount > 0) {
            Token token = CurrentToken(context);

            if (token.type == TokenType::LeftBrace) {
                braceCount++;
                context = Advance(context);
            }
            else if (token.type == TokenType::RightBrace) {
                braceCount--;
                if (braceCount == 0) {
                    foundClosingBrace = true;
                    break;
                }
                context = Advance(context);
            }
            else if (token.type == TokenType::Identifier && braceCount == 1) {
                // Parse tylko proste pary: type name;
                std::string type = token.value;
                context = Advance(context);

                if (!IsAtEnd(context) && CurrentToken(context).type == TokenType::Identifier) {
                    std::string name = CurrentToken(context).value;
                    bool isSampler = type.find("sampler") != std::string::npos;

                    result.inputVariables.push_back({ type, name, isSampler });
                    errorManager.ReportError("Found input variable: " + type + " " + name,
                        ErrorSeverity::Info, ErrorCategory::Parser, token.line, token.column, token.position, "", "");

                    context = Advance(context);

                    // Skip semicolon if present
                    if (!IsAtEnd(context) && CurrentToken(context).type == TokenType::Semicolon) {
                        context = Advance(context);
                    }
                }
                else {
                    ReportError("Expected variable name after type '" + type + "'", context);
                    context = Advance(context);
                }
            }
            else if (token.type == TokenType::Comment) {
                // Skip comments
                context = Advance(context);
            }
            else if (token.type == TokenType::Semicolon) {
                // Skip semicolons
                context = Advance(context);
            }
            else {
                // Skip other tokens but advance to avoid infinite loop
                context = Advance(context);
            }
        }

        if (!foundClosingBrace) {
            ReportError("Unterminated InputData block - missing closing brace '}'", context);
        }

        return context;
    }

    std::vector<InputVariable> ShaderParser::ParseInputDataFields(ParseContext& context) {
        std::vector<InputVariable> variables;

        context = Advance(context); // Skip opening brace

        while (!IsAtEnd(context) && CurrentToken(context).type != TokenType::RightBrace) {
            Token token = CurrentToken(context);

            if (token.type == TokenType::Identifier) {
                std::string type = token.value;
                context = Advance(context);

                if (!IsAtEnd(context) && CurrentToken(context).type == TokenType::Identifier) {
                    std::string name = CurrentToken(context).value;
                    bool isSampler = type.find("sampler") != std::string::npos;

                    variables.push_back({ type, name, isSampler });
                }
                else {
                    ReportError("Expected variable name after type '" + type + "'", context);
                }
            }
            else if (token.type == TokenType::Comment) {
                // Skip comments
            }
            else if (token.type != TokenType::Semicolon) {
                ReportError("Unexpected token in InputData block: " + token.value, context);
            }

            context = Advance(context);
        }

        if (IsAtEnd(context)) {
            ReportError("Unterminated InputData block", context);
        }

        return variables;
    }

    Token ShaderParser::CurrentToken(const ParseContext& context) {
        if (!ValidateTokenIndex(context, context.currentPosition)) {
            return context.tokens->back(); // Return EOF token
        }
        return (*context.tokens)[context.currentPosition];
    }

    Token ShaderParser::PeekToken(const ParseContext& context, size_t offset) {
        size_t peekPos = context.currentPosition + offset;
        if (!ValidateTokenIndex(context, peekPos)) {
            return context.tokens->back(); // Return EOF token
        }
        return (*context.tokens)[peekPos];
    }

    ParseContext ShaderParser::Advance(ParseContext context) {
        if (context.currentPosition < context.tokens->size() - 1) {
            context.currentPosition++;
        }
        return context;
    }

    bool ShaderParser::IsAtEnd(const ParseContext& context) {
        return context.currentPosition >= context.tokens->size() - 1 ||
            CurrentToken(context).type == TokenType::EndOfFile;
    }

    bool ShaderParser::IsValidContext(const ParseContext& context) {
        Token token = CurrentToken(context);
        return token.type != TokenType::Comment && token.type != TokenType::String;
    }

    void ShaderParser::ReportError(const std::string& message, const ParseContext& context) {
        Token token = CurrentToken(context);
        ReportError(message, token);
    }

    void ShaderParser::ReportError(const std::string& message, const Token& token) {
        errors_.push_back({
            message,
            token.line,
            token.column,
            token.position,
            "Parser at token: " + token.value
            });

        // Also report to error manager
        auto& errorManager = ShaderErrorManager::Instance();
        errorManager.ReportParserError(message, token.line, token.column, token.position,
            "Parser at token: " + token.value, "");
    }

    bool ShaderParser::ShouldContinue(const ParseContext& context) {
        return context.errors.size() < context.maxErrors && errors_.size() < maxErrors_;
    }

    ParseContext ShaderParser::RecoverFromError(ParseContext context) {
        // Try to recover to next safe point
        return SkipToNextStatement(context);
    }

    ParseContext ShaderParser::SkipToNextStatement(ParseContext context) {
        while (!IsAtEnd(context)) {
            Token token = CurrentToken(context);
            if (token.type == TokenType::Semicolon ||
                token.type == TokenType::RightBrace ||
                token.type == TokenType::LeftBrace) {
                break;
            }
            context = Advance(context);
        }
        return context;
    }

    ParseContext ShaderParser::SkipToNextBlock(ParseContext context) {
        int braceCount = 0;
        while (!IsAtEnd(context)) {
            Token token = CurrentToken(context);
            if (token.type == TokenType::LeftBrace) {
                braceCount++;
            }
            else if (token.type == TokenType::RightBrace) {
                braceCount--;
                if (braceCount <= 0) {
                    break;
                }
            }
            context = Advance(context);
        }
        return context;
    }

    bool ShaderParser::ValidateTokenIndex(const ParseContext& context, size_t index) {
        return index < context.tokens->size();
    }

    bool ShaderParser::ValidateShaderStages(const ParsedShaderData& data) {
        // Check if we have at least vertex and fragment stages
        bool hasVertex = false;
        bool hasFragment = false;

        for (const auto& stage : data.stages) {
            if (stage.stage == Stage::Vertex) {
                hasVertex = true;
            }
            else if (stage.stage == Stage::Fragment) {
                hasFragment = true;
            }
        }

        return hasVertex && hasFragment;
    }

} // namespace Shader