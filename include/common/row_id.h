#pragma once

#include <cstdint>

namespace db {

struct RowId {
    std::uint32_t page_id;
    std::uint16_t slot_index;
};

}  // namespace db
