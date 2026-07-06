#include "catalog/catalog_serializer.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace db::catalog {
namespace {

constexpr std::uint32_t CATALOG_MAGIC = 0x4341544C;  // "CATL"
constexpr std::uint32_t CATALOG_VERSION = 1;

void append_bytes(std::vector<char>& buffer, const void* data, std::size_t size) {
    const char* bytes = static_cast<const char*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

void append_u32(std::vector<char>& buffer, std::uint32_t value) {
    append_bytes(buffer, &value, sizeof(value));
}

void append_bool(std::vector<char>& buffer, bool value) {
    const std::uint8_t encoded = value ? 1 : 0;
    append_bytes(buffer, &encoded, sizeof(encoded));
}

void append_string(std::vector<char>& buffer, const std::string& value) {
    append_u32(buffer, static_cast<std::uint32_t>(value.size()));
    if (!value.empty()) {
        append_bytes(buffer, value.data(), value.size());
    }
}

bool read_u32(const char* data, std::size_t size, std::size_t& offset, std::uint32_t& value) {
    if (offset + sizeof(value) > size) {
        return false;
    }

    std::memcpy(&value, data + offset, sizeof(value));
    offset += sizeof(value);
    return true;
}

bool read_bool(const char* data, std::size_t size, std::size_t& offset, bool& value) {
    std::uint8_t encoded = 0;
    if (offset + sizeof(encoded) > size) {
        return false;
    }

    std::memcpy(&encoded, data + offset, sizeof(encoded));
    offset += sizeof(encoded);

    if (encoded != 0 && encoded != 1) {
        return false;
    }

    value = (encoded == 1);
    return true;
}

bool read_string(const char* data, std::size_t size, std::size_t& offset, std::string& value) {
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

void serialize_column(std::vector<char>& buffer, const ColumnDefinition& column) {
    append_string(buffer, column.name);
    append_u32(buffer, static_cast<std::uint32_t>(column.type));
    append_bool(buffer, column.is_primary_key);
    append_bool(buffer, column.is_auto_increment);
}

bool deserialize_column(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    ColumnDefinition& column
) {
    std::uint32_t raw_type = 0;

    if (!read_string(data, size, offset, column.name)) {
        return false;
    }

    if (!read_u32(data, size, offset, raw_type)) {
        return false;
    }

    if (raw_type > static_cast<std::uint32_t>(ColumnType::Date)) {
        return false;
    }

    column.type = static_cast<ColumnType>(raw_type);

    if (!read_bool(data, size, offset, column.is_primary_key)) {
        return false;
    }

    if (!read_bool(data, size, offset, column.is_auto_increment)) {
        return false;
    }

    return true;
}

void serialize_index(std::vector<char>& buffer, const IndexDefinition& index_definition) {
    append_string(buffer, index_definition.index_name);
    append_string(buffer, index_definition.table_name);
    append_string(buffer, index_definition.column_name);
    append_string(buffer, index_definition.file_name);
    append_bool(buffer, index_definition.is_primary);
    append_bool(buffer, index_definition.is_unique);
}

bool deserialize_index(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    IndexDefinition& index_definition
) {
    if (!read_string(data, size, offset, index_definition.index_name)) {
        return false;
    }

    if (!read_string(data, size, offset, index_definition.table_name)) {
        return false;
    }

    if (!read_string(data, size, offset, index_definition.column_name)) {
        return false;
    }

    if (!read_string(data, size, offset, index_definition.file_name)) {
        return false;
    }

    if (!read_bool(data, size, offset, index_definition.is_primary)) {
        return false;
    }

    if (!read_bool(data, size, offset, index_definition.is_unique)) {
        return false;
    }

    return true;
}

void serialize_table(std::vector<char>& buffer, const TableDefinition& table_definition) {
    append_string(buffer, table_definition.table_name);
    append_string(buffer, table_definition.heap_file_name);

    // Schema columns are written before index definitions so the table shape
    // is reconstructed before its attached indexes are loaded.
    const auto& columns = table_definition.schema.columns();
    append_u32(buffer, static_cast<std::uint32_t>(columns.size()));
    for (const ColumnDefinition& column : columns) {
        serialize_column(buffer, column);
    }

    append_u32(buffer, static_cast<std::uint32_t>(table_definition.indexes.size()));
    for (const IndexDefinition& index_definition : table_definition.indexes) {
        serialize_index(buffer, index_definition);
    }
}

bool deserialize_table(
    const char* data,
    std::size_t size,
    std::size_t& offset,
    TableDefinition& table_definition
) {
    if (!read_string(data, size, offset, table_definition.table_name)) {
        return false;
    }

    if (!read_string(data, size, offset, table_definition.heap_file_name)) {
        return false;
    }

    std::uint32_t column_count = 0;
    if (!read_u32(data, size, offset, column_count)) {
        return false;
    }

    std::vector<ColumnDefinition> columns;
    columns.reserve(column_count);

    for (std::uint32_t i = 0; i < column_count; ++i) {
        ColumnDefinition column;
        if (!deserialize_column(data, size, offset, column)) {
            return false;
        }
        columns.push_back(column);
    }

    table_definition.schema = Schema(std::move(columns));

    std::uint32_t index_count = 0;
    if (!read_u32(data, size, offset, index_count)) {
        return false;
    }

    table_definition.indexes.clear();
    table_definition.indexes.reserve(index_count);

    for (std::uint32_t i = 0; i < index_count; ++i) {
        IndexDefinition index_definition;
        if (!deserialize_index(data, size, offset, index_definition)) {
            return false;
        }
        table_definition.indexes.push_back(index_definition);
    }

    return true;
}

}  // namespace

std::vector<char> CatalogSerializer::serialize(const CatalogMetadata& metadata) {
    std::vector<char> buffer;

    // Prefix the blob with a magic number and version so newer code can reject
    // unknown or malformed catalog files safely.
    append_u32(buffer, CATALOG_MAGIC);
    append_u32(buffer, CATALOG_VERSION);
    append_u32(buffer, static_cast<std::uint32_t>(metadata.tables.size()));

    for (const TableDefinition& table_definition : metadata.tables) {
        serialize_table(buffer, table_definition);
    }

    return buffer;
}

std::optional<CatalogMetadata> CatalogSerializer::deserialize(const char* data, std::size_t size) {
    if (data == nullptr) {
        return std::nullopt;
    }

    std::size_t offset = 0;
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t table_count = 0;

    if (!read_u32(data, size, offset, magic) ||
        !read_u32(data, size, offset, version) ||
        !read_u32(data, size, offset, table_count)) {
        return std::nullopt;
    }

    if (magic != CATALOG_MAGIC || version != CATALOG_VERSION) {
        return std::nullopt;
    }

    CatalogMetadata metadata;
    metadata.tables.reserve(table_count);

    for (std::uint32_t i = 0; i < table_count; ++i) {
        TableDefinition table_definition;
        if (!deserialize_table(data, size, offset, table_definition)) {
            return std::nullopt;
        }

        metadata.tables.push_back(std::move(table_definition));
    }

    if (offset != size) {
        // Reject trailing garbage so the on-disk format stays strict.
        return std::nullopt;
    }

    return metadata;
}

}  // namespace db::catalog
