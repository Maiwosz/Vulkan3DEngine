#include "pch.h"
#include "ParserDictionary.h"
#include <TypeConversions.h>
#include <cctype>

namespace Shader {

    ParserDictionary::ParserDictionary() {
        InitializeHandlers();
    }

    void ParserDictionary::InitializeHandlers() {
        directiveHandlers_["#version"] = HandleVersionDirective;
        directiveHandlers_["#use"] = HandleUseDirective;
        directiveHandlers_["#stage"] = HandleStageDirective;
    }

    std::function<void(const Token&, ParsedShaderData&, const std::string&)>
        ParserDictionary::GetDirectiveHandler(const std::string& directive) const {
        auto it = directiveHandlers_.find(directive);
        return (it != directiveHandlers_.end()) ? it->second : nullptr;
    }

    bool ParserDictionary::IsDirectiveSupported(const std::string& directive) const {
        return directiveHandlers_.find(directive) != directiveHandlers_.end();
    }

    void ParserDictionary::HandleVersionDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource) {
        result.versionLine = token.value;
    }

    void ParserDictionary::HandleUseDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource) {
        std::string value = ExtractDirectiveValue(token, "#use");

        if (value == "global_ubo") {
            result.usesGlobalUBO = true;
        }
        else if (value == "object_ubo") {
            result.usesObjectUBO = true;
        }
    }

    void ParserDictionary::HandleStageDirective(const Token& token, ParsedShaderData& result, const std::string& originalSource) {
        std::string stageName = ExtractDirectiveValue(token, "#stage");

        if (!stageName.empty()) {
            Stage stage = ShaderLib::TypeConversion::StringToStage(stageName);

            // Find the start of the actual stage code (skip the directive line)
            size_t codeStart = token.position + token.value.length();

            // Skip any whitespace and newlines after the directive
            while (codeStart < originalSource.length() && std::isspace(originalSource[codeStart])) {
                codeStart++;
            }

            std::string code = ExtractStageCode(token, originalSource, {});
            result.stages.push_back({ code, stage });
        }
    }

    std::string ParserDictionary::ExtractDirectiveValue(const Token& token, const std::string& directive) {
        std::string value = token.value;

        if (value.length() > directive.length() + 1) {
            return value.substr(directive.length() + 1); // +1 for space
        }

        return "";
    }

    std::string ParserDictionary::ExtractStageCode(const Token& token, const std::string& originalSource, const std::vector<Token>& tokens) {
        size_t startPos = token.position + token.value.length();

        // Skip whitespace after directive
        while (startPos < originalSource.length() && std::isspace(originalSource[startPos])) {
            startPos++;
        }

        // Find the next #stage directive or end of file
        size_t endPos = originalSource.length();

        size_t searchPos = startPos;
        while (searchPos < originalSource.length()) {
            if (originalSource.substr(searchPos, 6) == "#stage") {
                endPos = searchPos;
                break;
            }
            searchPos++;
        }

        if (startPos < originalSource.length() && endPos <= originalSource.length()) {
            std::string stageCode = originalSource.substr(startPos, endPos - startPos);

            // Remove trailing whitespace
            while (!stageCode.empty() && std::isspace(stageCode.back())) {
                stageCode.pop_back();
            }

            return stageCode;
        }

        return "";
    }

} // namespace Shader