#pragma once
#include <string>
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
};

namespace std {
    template<> struct hash<AssetHandle> {
        size_t operator()(const AssetHandle& handle) const {
            return hash<std::string>()(handle.filename) ^ hash<int>()(static_cast<int>(handle.type));
        }
    };
}