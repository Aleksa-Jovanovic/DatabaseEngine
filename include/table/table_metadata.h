#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace db::table {

struct IndexMetadata {
    std::string index_name;
    std::string column_name;
    std::string file_name;
    bool is_primary = false;
    bool is_unique = true;
};

struct TableMetadata {
    std::string table_name;
    std::string heap_file_name;
    std::string primary_index_file_name;
    std::size_t cache_size = 8;
    std::vector<IndexMetadata> secondary_indexes;
};

}  // namespace db::table
