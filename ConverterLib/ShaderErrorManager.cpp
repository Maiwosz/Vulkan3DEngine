#include "pch.h"
#include "ShaderErrorManager.h"
#include <algorithm>
#include <iomanip>

namespace Shader {

    ShaderErrorManager& ShaderErrorManager::Instance() {
        static ShaderErrorManager instance;
        return instance;
    }

    void ShaderErrorManager::ReportError(const std::string& message, ErrorSeverity severity, ErrorCategory category,
        size_t line, size_t column, size_t position,
        const std::string& context, const std::string& filename) {
        if (ShouldContinue()) {
            errors_.emplace_back(message, severity, category, line, column, position, context, filename);
        }
    }

    void ShaderErrorManager::ReportLexerError(const std::string& message, size_t line, size_t column, size_t position,
        const std::string& context, const std::string& filename) {
        ReportError(message, ErrorSeverity::Error, ErrorCategory::Lexer, line, column, position, context, filename);
    }

    void ShaderErrorManager::ReportParserError(const std::string& message, size_t line, size_t column, size_t position,
        const std::string& context, const std::string& filename) {
        ReportError(message, ErrorSeverity::Error, ErrorCategory::Parser, line, column, position, context, filename);
    }

    void ShaderErrorManager::ReportSemanticError(const std::string& message, size_t line, size_t column, size_t position,
        const std::string& context, const std::string& filename) {
        ReportError(message, ErrorSeverity::Error, ErrorCategory::Semantic, line, column, position, context, filename);
    }

    void ShaderErrorManager::ReportValidationError(const std::string& message, const std::string& context,
        const std::string& filename) {
        ReportError(message, ErrorSeverity::Error, ErrorCategory::Validation, 0, 0, 0, context, filename);
    }

    std::vector<ErrorInfo> ShaderErrorManager::GetErrorsByCategory(ErrorCategory category) const {
        std::vector<ErrorInfo> filtered;
        std::copy_if(errors_.begin(), errors_.end(), std::back_inserter(filtered),
            [category](const ErrorInfo& error) { return error.category == category; });
        return filtered;
    }

    std::vector<ErrorInfo> ShaderErrorManager::GetErrorsBySeverity(ErrorSeverity severity) const {
        std::vector<ErrorInfo> filtered;
        std::copy_if(errors_.begin(), errors_.end(), std::back_inserter(filtered),
            [severity](const ErrorInfo& error) { return error.severity == severity; });
        return filtered;
    }

    size_t ShaderErrorManager::GetErrorCount(ErrorSeverity severity) const {
        return std::count_if(errors_.begin(), errors_.end(),
            [severity](const ErrorInfo& error) { return error.severity == severity; });
    }

    size_t ShaderErrorManager::GetErrorCount(ErrorCategory category) const {
        return std::count_if(errors_.begin(), errors_.end(),
            [category](const ErrorInfo& error) { return error.category == category; });
    }

    bool ShaderErrorManager::HasRealErrors() const {
        for (const auto& error : errors_) {
            if (error.severity == ErrorSeverity::Error || error.severity == ErrorSeverity::Fatal) {
                return true;
            }
        }
        return false;
    }

    std::string ShaderErrorManager::FormatError(const ErrorInfo& error) const {
        std::stringstream ss;

        if (colorOutput_) {
            ss << GetColorCode(error.severity);
        }

        // Format: [SEVERITY] [CATEGORY] message (line:column)
        ss << "[" << SeverityToString(error.severity) << "] ";
        ss << "[" << CategoryToString(error.category) << "] ";
        ss << error.message;

        if (error.line > 0) {
            ss << " (line " << error.line;
            if (error.column > 0) {
                ss << ", column " << error.column;
            }
            ss << ")";
        }

        if (!error.filename.empty()) {
            ss << " in " << error.filename;
        }

        if (showContext_ && !error.context.empty()) {
            ss << "\n    Context: " << error.context;
        }

        if (colorOutput_) {
            ss << GetResetCode();
        }

        return ss.str();
    }

    std::string ShaderErrorManager::FormatAllErrors() const {
        std::stringstream ss;

        for (const auto& error : errors_) {
            ss << FormatError(error) << "\n";
        }

        return ss.str();
    }

    std::string ShaderErrorManager::FormatErrorsSummary() const {
        std::stringstream ss;

        size_t totalErrors = GetErrorCount();
        size_t fatalErrors = GetErrorCount(ErrorSeverity::Fatal);
        size_t errors = GetErrorCount(ErrorSeverity::Error);
        size_t warnings = GetErrorCount(ErrorSeverity::Warning);
        size_t infos = GetErrorCount(ErrorSeverity::Info);

        ss << "Error Summary: " << totalErrors << " total";

        if (fatalErrors > 0) {
            ss << ", " << fatalErrors << " fatal";
        }
        if (errors > 0) {
            ss << ", " << errors << " errors";
        }
        if (warnings > 0) {
            ss << ", " << warnings << " warnings";
        }
        if (infos > 0) {
            ss << ", " << infos << " info";
        }

        return ss.str();
    }

    std::string ShaderErrorManager::FormatNonInfoErrors() const {
        std::stringstream ss;

        for (const auto& error : errors_) {
            if (error.severity != ErrorSeverity::Info) {
                ss << FormatError(error) << "\n";
            }
        }

        return ss.str();
    }

    std::string ShaderErrorManager::SeverityToString(ErrorSeverity severity) const {
        switch (severity) {
        case ErrorSeverity::Info:    return "INFO";
        case ErrorSeverity::Warning: return "WARNING";
        case ErrorSeverity::Error:   return "ERROR";
        case ErrorSeverity::Fatal:   return "FATAL";
        default:                     return "UNKNOWN";
        }
    }

    std::string ShaderErrorManager::CategoryToString(ErrorCategory category) const {
        switch (category) {
        case ErrorCategory::Lexer:     return "LEXER";
        case ErrorCategory::Parser:    return "PARSER";
        case ErrorCategory::Semantic:  return "SEMANTIC";
        case ErrorCategory::Validation: return "VALIDATION";
        case ErrorCategory::IO:        return "IO";
        default:                       return "UNKNOWN";
        }
    }

    std::string ShaderErrorManager::GetColorCode(ErrorSeverity severity) const {
        if (!colorOutput_) return "";

        switch (severity) {
        case ErrorSeverity::Info:    return "\033[36m"; // Cyan
        case ErrorSeverity::Warning: return "\033[33m"; // Yellow
        case ErrorSeverity::Error:   return "\033[31m"; // Red
        case ErrorSeverity::Fatal:   return "\033[35m"; // Magenta
        default:                     return "";
        }
    }

    std::string ShaderErrorManager::GetResetCode() const {
        return colorOutput_ ? "\033[0m" : "";
    }

} // namespace Shader