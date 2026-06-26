#include "table/row_serializer.h"

#include <cstring>
#include <string>

namespace db::table {
namespace {

enum class FieldTypeTag : std::uint8_t {
    Integer = 1,
    String = 2,
    Boolean = 3,
    Date = 4
};

void append_bytes(
    std::vector<char>& buffer,
    const void* data,
    std::size_t size
) {
    const auto* bytes = static_cast<const char*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

void append_u8(std::vector<char>& buffer, std::uint8_t value) {
    append_bytes(buffer, &value, sizeof(value));
}

void append_u32(std::vector<char>& buffer, std::uint32_t value) {
    append_bytes(buffer, &value, sizeof(value));
}

void append_i64(std::vector<char>& buffer, std::int64_t value) {
    append_bytes(buffer, &value, sizeof(value));
}

void append_string(std::vector<char>& buffer, const std::string& value) {
    append_u32(buffer, static_cast<std::uint32_t>(value.size()));

    if (!value.empty()) {
        append_bytes(buffer, value.data(), value.size());
    }
}

bool read_bytes(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    void* output,
    std::size_t output_size
) {
    if (offset + output_size > size) {
        return false;
    }

    std::memcpy(output, data + offset, output_size);
    offset += output_size;
    return true;
}

bool read_u8(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    std::uint8_t& value
) {
    return read_bytes(data, size, offset, &value, sizeof(value));
}

bool read_u32(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    std::uint32_t& value
) {
    return read_bytes(data, size, offset, &value, sizeof(value));
}

bool read_i64(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    std::int64_t& value
) {
    return read_bytes(data, size, offset, &value, sizeof(value));
}

bool read_string(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    std::string& value
) {
    std::uint32_t length = 0;

    if (!read_u32(data, size, offset, length)) {
        return false;
    }

    if (offset + length > size) {
        return false;
    }

    value.assign(data + offset, data + offset + length);
    offset += length;
    return true;
}

}  // namespace

std::vector<char> RowSerializer::serialize(const Row& row) {
    std::vector<char> buffer;

    // [field_count]
    append_u32(buffer, static_cast<std::uint32_t>(row.values.size()));

    // for each field:
    // [type_tag]
    // [field_payload]
    for (const FieldValue& field : row.values) {
        if (std::holds_alternative<std::int64_t>(field)) {
            append_u8(buffer, static_cast<std::uint8_t>(FieldTypeTag::Integer));
            append_i64(buffer, std::get<std::int64_t>(field));
            continue;
        }

        if (std::holds_alternative<std::string>(field)) {
            append_u8(buffer, static_cast<std::uint8_t>(FieldTypeTag::String));
            append_string(buffer, std::get<std::string>(field));
            continue;
        }

        if (std::holds_alternative<bool>(field)) {
            append_u8(buffer, static_cast<std::uint8_t>(FieldTypeTag::Boolean));
            append_u8(buffer, std::get<bool>(field) ? 1 : 0);
            continue;
        }

        if (std::holds_alternative<DateValue>(field)) {
            append_u8(buffer, static_cast<std::uint8_t>(FieldTypeTag::Date));
            append_string(buffer, std::get<DateValue>(field).value);
            continue;
        }
    }

    return buffer;
}

std::optional<Row> RowSerializer::deserialize(
    std::uint32_t key,
    const char* data, 
    std::size_t size
) {
    if (data == nullptr) {
        return std::nullopt;
    }

    std::size_t offset = 0;
    std::uint32_t field_count = 0;

    if (!read_u32(data, size, offset, field_count)) {
        return std::nullopt;
    }

    Row row{};
    row.key = key;
    row.values.reserve(field_count);

    for (std::uint32_t i = 0; i < field_count; i++) {
        std::uint8_t raw_type_tag = 0;
        
        if (!read_u8(data, size, offset, raw_type_tag)) {
            return std::nullopt;
        }

        const auto type_tag = static_cast<FieldTypeTag>(raw_type_tag);

        switch (type_tag) {
            case FieldTypeTag::Integer: {
                std::int64_t value = 0;

                if (!read_i64(data, size, offset, value)) {
                    return std::nullopt;
                }

                row.values.push_back(value);
                break;
            }

            case FieldTypeTag::String: {
                std::string value;

                if (!read_string(data, size, offset, value)) {
                    return std::nullopt;
                }

                row.values.push_back(value);
                break;
            }

            case FieldTypeTag::Boolean: {
                std::uint8_t value = 0;

                if (!read_u8(data, size, offset, value)) {
                    return std::nullopt;
                }

                if (value > 1) {
                    return std::nullopt;
                }

                row.values.push_back(value == 1);
                break;
            }

            case FieldTypeTag::Date: {
                std::string value;

                if (!read_string(data, size, offset, value)) {
                    return std::nullopt;
                }

                row.values.push_back(DateValue{value});
                break;
            }

            default:
                return std::nullopt;
        }
    }

    if (offset != size) {
        return std::nullopt;
    }

    return row;
}

}  // namespace db::table