#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <glm/glm.hpp>

class BinaryWriter {
public:
    BinaryWriter() = default;

    // Write primitive types
    void write(bool value) {
        uint8_t val = value ? 1 : 0;
        m_data.push_back(val);
    }

    void write(uint8_t value) {
        m_data.push_back(value);
    }

    void write(uint16_t value) {
        m_data.resize(m_data.size() + sizeof(uint16_t));
        std::memcpy(m_data.data() + m_data.size() - sizeof(uint16_t), &value, sizeof(uint16_t));
    }

    void write(uint32_t value) {
        m_data.resize(m_data.size() + sizeof(uint32_t));
        std::memcpy(m_data.data() + m_data.size() - sizeof(uint32_t), &value, sizeof(uint32_t));
    }

    void write(uint64_t value) {
        m_data.resize(m_data.size() + sizeof(uint64_t));
        std::memcpy(m_data.data() + m_data.size() - sizeof(uint64_t), &value, sizeof(uint64_t));
    }

    void write(int8_t value) {
        m_data.push_back(static_cast<uint8_t>(value));
    }

    void write(int16_t value) {
        write(static_cast<uint16_t>(value));
    }

    void write(int32_t value) {
        write(static_cast<uint32_t>(value));
    }

    void write(int64_t value) {
        write(static_cast<uint64_t>(value));
    }

    void write(float value) {
        m_data.resize(m_data.size() + sizeof(float));
        std::memcpy(m_data.data() + m_data.size() - sizeof(float), &value, sizeof(float));
    }

    void write(double value) {
        m_data.resize(m_data.size() + sizeof(double));
        std::memcpy(m_data.data() + m_data.size() - sizeof(double), &value, sizeof(double));
    }

    // Write string (length + data)
    void write(const std::string& value) {
        write(static_cast<uint32_t>(value.size()));
        if (!value.empty()) {
            size_t oldSize = m_data.size();
            m_data.resize(oldSize + value.size());
            std::memcpy(m_data.data() + oldSize, value.data(), value.size());
        }
    }

    // Write GLM types
    void write(const glm::vec3& value) {
        write(value.x);
        write(value.y);
        write(value.z);
    }

    void write(const glm::vec4& value) {
        write(value.x);
        write(value.y);
        write(value.z);
        write(value.w);
    }

    // Write raw data
    void writeRaw(const uint8_t* data, size_t size) {
        size_t oldSize = m_data.size();
        m_data.resize(oldSize + size);
        std::memcpy(m_data.data() + oldSize, data, size);
    }

    // Get the data
    const std::vector<uint8_t>& getData() const {
        return m_data;
    }

    // Get size
    size_t size() const {
        return m_data.size();
    }

    // Clear data
    void clear() {
        m_data.clear();
    }

private:
    std::vector<uint8_t> m_data;
};

class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size), m_position(0) {
    }

    // Check if we can read more data
    bool canRead(size_t bytes) const {
        return m_position + bytes <= m_size;
    }

    // Read primitive types
    bool read(bool& value) {
        if (!canRead(1)) return false;
        value = m_data[m_position] != 0;
        m_position += 1;
        return true;
    }

    bool read(uint8_t& value) {
        if (!canRead(1)) return false;
        value = m_data[m_position];
        m_position += 1;
        return true;
    }

    bool read(uint16_t& value) {
        if (!canRead(sizeof(uint16_t))) return false;
        std::memcpy(&value, m_data + m_position, sizeof(uint16_t));
        m_position += sizeof(uint16_t);
        return true;
    }

    bool read(uint32_t& value) {
        if (!canRead(sizeof(uint32_t))) return false;
        std::memcpy(&value, m_data + m_position, sizeof(uint32_t));
        m_position += sizeof(uint32_t);
        return true;
    }

    bool read(uint64_t& value) {
        if (!canRead(sizeof(uint64_t))) return false;
        std::memcpy(&value, m_data + m_position, sizeof(uint64_t));
        m_position += sizeof(uint64_t);
        return true;
    }

    bool read(int8_t& value) {
        uint8_t temp;
        if (!read(temp)) return false;
        value = static_cast<int8_t>(temp);
        return true;
    }

    bool read(int16_t& value) {
        uint16_t temp;
        if (!read(temp)) return false;
        value = static_cast<int16_t>(temp);
        return true;
    }

    bool read(int32_t& value) {
        uint32_t temp;
        if (!read(temp)) return false;
        value = static_cast<int32_t>(temp);
        return true;
    }

    bool read(int64_t& value) {
        uint64_t temp;
        if (!read(temp)) return false;
        value = static_cast<int64_t>(temp);
        return true;
    }

    bool read(float& value) {
        if (!canRead(sizeof(float))) return false;
        std::memcpy(&value, m_data + m_position, sizeof(float));
        m_position += sizeof(float);
        return true;
    }

    bool read(double& value) {
        if (!canRead(sizeof(double))) return false;
        std::memcpy(&value, m_data + m_position, sizeof(double));
        m_position += sizeof(double);
        return true;
    }

    // Read string (length + data)
    bool read(std::string& value) {
        uint32_t length;
        if (!read(length)) return false;

        if (length == 0) {
            value.clear();
            return true;
        }

        if (!canRead(length)) return false;

        value.assign(reinterpret_cast<const char*>(m_data + m_position), length);
        m_position += length;
        return true;
    }

    // Read GLM types
    bool read(glm::vec3& value) {
        return read(value.x) && read(value.y) && read(value.z);
    }

    bool read(glm::vec4& value) {
        return read(value.x) && read(value.y) && read(value.z) && read(value.w);
    }

    // Read raw data
    bool readRaw(uint8_t* buffer, size_t size) {
        if (!canRead(size)) return false;
        std::memcpy(buffer, m_data + m_position, size);
        m_position += size;
        return true;
    }

    // Get current position
    size_t getPosition() const {
        return m_position;
    }

    // Get remaining bytes
    size_t getRemainingBytes() const {
        return m_size - m_position;
    }

    // Skip bytes
    bool skip(size_t bytes) {
        if (!canRead(bytes)) return false;
        m_position += bytes;
        return true;
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_position;
};