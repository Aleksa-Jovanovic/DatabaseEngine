#pragma once

#include <cstdint>

namespace db {

//For structs that describe how bytes inside a page are organized
struct PageHeader {
    std::uint32_t num_records;
};

// Header for the slotted-page format used in Phase 3.
// Free space is treated as the half-open range:
// [free_space_start, free_space_end)
struct SlottedPageHeader {
    std::uint16_t slot_count;
    std::uint16_t free_space_start;
    std::uint16_t free_space_end;
};

// Each slot points to one record's bytes inside the page.
struct SlotEntry {
    std::uint16_t offset;
    std::uint16_t length;
};

} // namespace db