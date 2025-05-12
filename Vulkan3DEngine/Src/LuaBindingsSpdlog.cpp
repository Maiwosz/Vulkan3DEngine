#include "LuaBindingsSpdlog.h"
#include <spdlog/spdlog.h>

namespace LuaBindings {

    // Helper function to format strings with variable arguments from Lua
    static std::string formatLuaString(const std::string& format, sol::variadic_args va) {
        std::string result;
        size_t arg_idx = 0;
        size_t start_idx = 0;
        size_t format_idx = 0;

        while (format_idx < format.size()) {
            if (format[format_idx] == '{' && format_idx + 1 < format.size() && format[format_idx + 1] == '}') {
                // Append substring before the placeholder
                if (format_idx > start_idx) {
                    result.append(format, start_idx, format_idx - start_idx);
                }

                // Process argument if there's one available
                if (arg_idx < va.size()) {
                    sol::object arg = va[arg_idx];
                    arg_idx++;

                    // Handle different types
                    if (arg.is<int>()) {
                        result += fmt::format("{}", arg.as<int>());
                    }
                    else if (arg.is<float>()) {
                        result += fmt::format("{:.6f}", arg.as<float>());
                    }
                    else if (arg.is<double>()) {
                        result += fmt::format("{:.6f}", arg.as<double>());
                    }
                    else if (arg.is<bool>()) {
                        result += fmt::format("{}", arg.as<bool>() ? "true" : "false");
                    }
                    else if (arg.is<std::string>()) {
                        result += fmt::format("{}", arg.as<std::string>());
                    }
                    else {
                        result += "[UNKNOWN TYPE]";
                    }
                }
                else {
                    result += "[MISSING ARG]";
                }

                format_idx += 2; // Skip {}
                start_idx = format_idx;
            }
            else {
                format_idx++;
            }
        }

        // Append remaining text after the last placeholder
        if (start_idx < format.size()) {
            result.append(format, start_idx, format.size() - start_idx);
        }

        return result;
    }

    void registerSpdlogFunctions(sol::state& state) {
        // Create Logger namespace in Lua
        auto loggerNS = state.create_named_table("Logger");

        // Register spdlog functions with different severity levels
        loggerNS.set_function("trace", [](const std::string& message) {
            SPDLOG_TRACE(message);
            });

        loggerNS.set_function("debug", [](const std::string& message) {
            SPDLOG_DEBUG(message);
            });

        loggerNS.set_function("info", [](const std::string& message) {
            SPDLOG_INFO(message);
            });

        loggerNS.set_function("warn", [](const std::string& message) {
            SPDLOG_WARN(message);
            });

        loggerNS.set_function("error", [](const std::string& message) {
            SPDLOG_ERROR(message);
            });

        loggerNS.set_function("critical", [](const std::string& message) {
            SPDLOG_CRITICAL(message);
            });

        // Formatted versions
        loggerNS.set_function("tracef", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_TRACE(formattedMsg);
            });

        loggerNS.set_function("debugf", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_DEBUG(formattedMsg);
            });

        loggerNS.set_function("infof", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_INFO(formattedMsg);
            });

        loggerNS.set_function("warnf", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_WARN(formattedMsg);
            });

        loggerNS.set_function("errorf", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_ERROR(formattedMsg);
            });

        loggerNS.set_function("criticalf", [](const std::string& format, sol::variadic_args va) {
            std::string formattedMsg = formatLuaString(format, va);
            SPDLOG_CRITICAL(formattedMsg);
            });
    }
}