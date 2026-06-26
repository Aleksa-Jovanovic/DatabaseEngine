#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "table/row.h"

namespace db::table {

class RowSerializer {
public:
    // [field_count]
    // for each field:
        // [type_tag]
        // [field_payload]
    
    // Serializes only Row::values. The primary key is stored separately in VarRecord::key.
    static std::vector<char> serialize(const Row& row);
    
    // Reattaches the key from VarRecord::key to the deserialized field values.
    static std::optional<Row> deserialize(
        std::uint32_t key,
        const char* data,
        std::size_t size
    );
};

}  // namespace db::table