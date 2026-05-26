#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "table/row.h"

namespace db::table {

class RowSerializer {
public:
    // [key][value_length][value_bytes]
    static std::vector<char> serialize(const Row& row);
    static std::optional<Row> deserialize(const char* data, std::size_t size);
};

}  // namespace db::table