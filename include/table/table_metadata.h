#pragma once

#include <cstddef>
#include <string>

namespace db::table {

struct TableMetadata {
    std::string table_name;
    std::string heap_file_name;
    std::string primary_index_file_name;
    std::size_t cache_size = 8;
};

}  // namespace db::table