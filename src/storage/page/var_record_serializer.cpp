#include "storage/page/var_record_serializer.h"

#include <cstring>

namespace db {

namespace {
constexpr std::size_t KEY_SIZE = sizeof(std::uint32_t);
constexpr std::size_t LENGTH_SIZE = sizeof(std::uint16_t);
constexpr std::size_t HEADER_SIZE = KEY_SIZE + LENGTH_SIZE;
}   // namespace

std::uint16_t serialized_size(const VarRecord &record) {
    return static_cast<std::uint16_t>(HEADER_SIZE + record.value.size());
}

std::vector<char> serialize_var_record(const VarRecord &record) {
    const std::uint16_t value_length = static_cast<std::uint16_t>(record.value.size());
    const std::uint16_t total_size = serialized_size(record);

    std::vector<char> buffer(total_size);

    std::memcpy(buffer.data(), &record.key, KEY_SIZE);
    std::memcpy(buffer.data() + KEY_SIZE, &value_length, LENGTH_SIZE);
    std::memcpy(buffer.data() + HEADER_SIZE, record.value.data(), value_length);

    return buffer;
}

std::optional<VarRecord> deserialize_var_record(const char *data, std::uint16_t length) {
    if (length < HEADER_SIZE) {
        return std::nullopt;
    }

    VarRecord record{};

    std::uint16_t value_length = 0;

    std::memcpy(&record.key, data, KEY_SIZE);
    std::memcpy(&value_length, data + KEY_SIZE, LENGTH_SIZE);

    if (length != HEADER_SIZE + value_length) {
        return std::nullopt;
    }

    record.value.assign(data + HEADER_SIZE, data + HEADER_SIZE + value_length);
    return record;
}

}   // namespace db