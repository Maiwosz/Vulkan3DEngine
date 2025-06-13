#pragma once
#include <vector>
#include <cstdint>

class IBinarySerializable {
public:
    virtual ~IBinarySerializable() = default;

    // Serialize object to binary data
    virtual std::vector<uint8_t> serializeBinary() const = 0;

    // Deserialize object from binary data
    // Returns number of bytes consumed, or 0 on error
    virtual size_t deserializeBinary(const uint8_t* data, size_t size) = 0;
};