#include "ConverterLib.h"
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input> <output> [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -format [RGBA8|BC7]  Texture format (default: RGBA8)\n";
        return 1;
    }

    Converter::Settings settings;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-format") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "BC7") == 0) {
                settings.textureFormat = AssetLib::TextureFormat::BC7;
            }
            i++;
        }
    }

    try {
        Converter converter;
        converter.Convert(argv[1], argv[2], settings);
        std::cout << "Successfully converted: " << argv[1] << " -> " << argv[2] << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}