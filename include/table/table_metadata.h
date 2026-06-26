#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace db::table {

enum class ColumnType {
    Integer,
    String,
    Boolean,
    Date
};

struct ColumnMetadata {
    std::string name;
    ColumnType type;
    bool is_primary_key = false;
};

struct IndexMetadata {
    std::string index_name;
    std::string column_name;
    std::string file_name;
    bool is_primary = false;
    bool is_unique = true;
};

// TableMetadata is the table-local runtime metadata used to construct a Table.
// Later, the catalog will store the persistent/global table definitions from
// which TableMetadata instances are derived when table objects are rebuilt.
struct TableMetadata {
    std::string table_name;
    std::string heap_file_name;
    std::string primary_index_file_name;
    std::size_t cache_size = 8;
    std::vector<ColumnMetadata> columns;
    std::vector<IndexMetadata> secondary_indexes;
};

}  // namespace db::table
