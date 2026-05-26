#include "table/row_serializer.h"

#include <cstring>

namespace db::table {

std::vector<char> RowSerializer::serialize(const Row& row) {
    const std::uint32_t value_length =
        static_cast<std::uint32_t>(row.value.size());

    const std::size_t total_size =
        sizeof(row.key) + sizeof(value_length) + value_length;

    std::vector<char> buffer(total_size);
    std::size_t offset = 0;

    std::memcpy(buffer.data() + offset, &row.key, sizeof(row.key));
    offset += sizeof(row.key);

    std::memcpy(buffer.data() + offset, &value_length, sizeof(value_length));
    offset += sizeof(value_length);

    if (value_length > 0) {
        std::memcpy(buffer.data() + offset, row.value.data(), value_length);
    }

    return buffer;
}

std::optional<Row> RowSerializer::deserialize(const char* data, std::size_t size) {
    if (data == nullptr) {
        return std::nullopt;
    }

    const std::size_t header_size =
        sizeof(std::uint32_t) + sizeof(std::uint32_t);

    if (size < header_size) {
        return std::nullopt;
    }

    Row row{};
    std::size_t offset = 0;
    std::uint32_t value_length = 0;

    std::memcpy(&row.key, data + offset, sizeof(row.key));
    offset += sizeof(row.key);

    std::memcpy(&value_length, data + offset, sizeof(value_length));
    offset += sizeof(value_length);

    if (size != header_size + value_length) {
        return std::nullopt;
    }

    row.value.assign(data + offset, data + offset + value_length);
    return row;
}

}  // namespace db::table