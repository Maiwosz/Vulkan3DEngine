#include "pch.h"
#include "Shader.h"
#include "ShaderParserPEGTL.h"
#include "ShaderCompiler.h"
#include <fstream>
#include <stdexcept>

using namespace ShaderLib;

// Global flag for debug printf (requires Vulkan configurator)
bool usePritnf = false;

namespace Shader {

    ShaderData CompileShader(const std::string& source) {
        // Parse the source using PEGTL parser
        ShaderParserPEGTL parser;
        ParsedShaderData parsedData;

        try {
            parsedData = parser.Parse(source);
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string("Shader parsing failed: ") + e.what());
        }

        // Compile using ShaderCompiler
        ShaderCompiler compiler;
        compiler.SetDebugPrintf(usePritnf);

        return compiler.Compile(parsedData);
    }

    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings) {
        std::ifstream file(inputPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + inputPath);
        }

        std::string source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        ShaderData shaderData = CompileShader(source);

        std::string filename = std::filesystem::path(inputPath).filename().string();

        return AssetLib::WriteShader(
            filename,
            shaderData,
            AssetLib::CompressionType::LZ4,
            settings.compressionLevel
        );
    }

} // namespace Shader