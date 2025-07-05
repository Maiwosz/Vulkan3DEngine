#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace Shader {

    enum class ErrorSeverity {
        Info,
        Warning,
        Error,
        Fatal
    };

    enum class ErrorCategory {
        Lexer,
        Parser,
        Semantic,
        Validation,
        IO
    };

    struct ErrorInfo {
        std::string message;
        ErrorSeverity severity;
        ErrorCategory category;
        size_t line;
        size_t column;
        size_t position;
        std::string context;
        std::string filename;

        ErrorInfo(const std::string& msg, ErrorSeverity sev, ErrorCategory cat,
            size_t ln = 0, size_t col = 0, size_t pos = 0,
            const std::string& ctx = "", const std::string& file = "")
            : message(msg), severity(sev), category(cat), line(ln),
            column(col), position(pos), context(ctx), filename(file) {
        }
    };

    class ShaderErrorManager {
    public:
        static ShaderErrorManager& Instance();

        // Error reporting methods
        void ReportError(const std::string& message, ErrorSeverity severity, ErrorCategory category,
            size_t line = 0, size_t column = 0, size_t position = 0,
            const std::string& context = "", const std::string& filename = "");

        void ReportLexerError(const std::string& message, size_t line, size_t column, size_t position,
            const std::string& context = "", const std::string& filename = "");

        void ReportParserError(const std::string& message, size_t line, size_t column, size_t position,
            const std::string& context = "", const std::string& filename = "");

        void ReportSemanticError(const std::string& message, size_t line, size_t column, size_t position,
            const std::string& context = "", const std::string& filename = "");

        void ReportValidationError(const std::string& message, const std::string& context = "",
            const std::string& filename = "");

        // Error retrieval
        const std::vector<ErrorInfo>& GetErrors() const { return errors_; }
        std::vector<ErrorInfo> GetErrorsByCategory(ErrorCategory category) const;
        std::vector<ErrorInfo> GetErrorsBySeverity(ErrorSeverity severity) const;

        // Error statistics
        size_t GetErrorCount() const { return errors_.size(); }
        size_t GetErrorCount(ErrorSeverity severity) const;
        size_t GetErrorCount(ErrorCategory category) const;

        bool HasErrors() const { return !errors_.empty(); }
        bool HasRealErrors() const;
        bool HasFatalErrors() const { return GetErrorCount(ErrorSeverity::Fatal) > 0; }
        bool HasNonWarningErrors() const {
            return GetErrorCount(ErrorSeverity::Error) > 0 || HasFatalErrors();
        }

        // Error formatting
        std::string FormatError(const ErrorInfo& error) const;
        std::string FormatAllErrors() const;
        std::string FormatErrorsSummary() const;
        std::string FormatNonInfoErrors() const;

        // Management
        void Clear() { errors_.clear(); }
        void SetMaxErrors(size_t maxErrors) { maxErrors_ = maxErrors; }
        bool ShouldContinue() const { return errors_.size() < maxErrors_; }

        // Configuration
        void SetShowContext(bool show) { showContext_ = show; }
        void SetColorOutput(bool enable) { colorOutput_ = enable; }

    private:
        ShaderErrorManager() : maxErrors_(50), showContext_(true), colorOutput_(false) {}

        std::vector<ErrorInfo> errors_;
        size_t maxErrors_;
        bool showContext_;
        bool colorOutput_;

        // Utility methods
        std::string SeverityToString(ErrorSeverity severity) const;
        std::string CategoryToString(ErrorCategory category) const;
        std::string GetColorCode(ErrorSeverity severity) const;
        std::string GetResetCode() const;
    };

    // Convenience macros for error reporting
#define REPORT_LEXER_ERROR(msg, line, col, pos) \
        ShaderErrorManager::Instance().ReportLexerError(msg, line, col, pos, __FUNCTION__, __FILE__)

#define REPORT_PARSER_ERROR(msg, line, col, pos) \
        ShaderErrorManager::Instance().ReportParserError(msg, line, col, pos, __FUNCTION__, __FILE__)

#define REPORT_SEMANTIC_ERROR(msg, line, col, pos) \
        ShaderErrorManager::Instance().ReportSemanticError(msg, line, col, pos, __FUNCTION__, __FILE__)

#define REPORT_VALIDATION_ERROR(msg) \
        ShaderErrorManager::Instance().ReportValidationError(msg, __FUNCTION__, __FILE__)

} // namespace Shader