#pragma once
#include <ShaderLib.h>
#include "ConverterLib.h"

using namespace ShaderLib;

namespace Shader {

    // Structure to store InputData definition information
    struct InputDataDefinition {
        struct Field {
            std::string type;
            std::string name;
            bool isTexture;
        };
        std::vector<Field> fields;
    };

    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings);

    // Core processing functions
    ShaderData CompileShader(const std::string& source);
}
