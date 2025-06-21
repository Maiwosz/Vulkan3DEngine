#pragma once
#include <string>
#include <string_view>
#include <AssetLib.h>

using AssetType = AssetLib::AssetType;

struct AssetHandle {
    AssetType type;
    std::string filename;

    AssetHandle() = default;
    AssetHandle(AssetType type, const std::string& filename)
        : type(type), filename(filename) {
    }

    bool operator==(const AssetHandle& other) const {
        return type == other.type && filename == other.filename;
    }

    // Zwraca nazwę typu zasobu
    std::string_view GetTypeName() const {
        return AssetLib::Utilities::GetAssetTypeName(type);
    }

    // Zwraca rozszerzenie pliku dla tego typu zasobu
    std::string_view GetFileExtension() const {
        return AssetLib::Utilities::GetAssetExtension(type);
    }

    // Zwraca pełną informację o uchwycie jako string (przydatne do debugowania)
    std::string ToString() const {
        return std::string(GetTypeName()) + ": " + filename;
    }
};

namespace std {
    template<> struct hash<AssetHandle> {
        size_t operator()(const AssetHandle& handle) const {
            return hash<std::string>()(handle.filename) ^ hash<int>()(static_cast<int>(handle.type));
        }
    };
}