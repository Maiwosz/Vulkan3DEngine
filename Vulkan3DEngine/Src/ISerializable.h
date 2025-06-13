#pragma once
#include <json.hpp>

using json = nlohmann::json;

class ISerializable {
public:
    virtual ~ISerializable() = default;

    // Serialize object to JSON
    virtual json serialize() const = 0;

    // Deserialize object from JSON
    virtual void deserialize(const json& j) = 0;
};