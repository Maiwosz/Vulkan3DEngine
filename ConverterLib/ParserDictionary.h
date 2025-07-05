#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "ShaderParser.h"

namespace Shader {

    class ParserDictionary {
    public:
        ParserDictionary();

        // Get handler for a directive
        std::function<void(const Token&, ParsedShaderData&, const std::string&)> GetDirectiveHandler(const std::string& directive) const;

        // Check if directive is supported
        bool IsDirectiveSupported(const std::string& directive) const;

    private:
        std::unordered_map<std::string, std::function<void(const Token&, ParsedShaderData&, const std::string&)>> directiveHandlers_;

        void InitializeHandlers();

        // Handler implementations
        static void HandleVersionDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource);
        static void HandleUseDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource);
        static void HandleStageDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource);

        // Utility methods
        static std::string ExtractDirectiveValue(const Token& token, const std::string& directive);
        static std::string ExtractStageCode(const Token& token, const std::string& originalSource, const std::vector<Token>& tokens);
    };

} // namespace Shader