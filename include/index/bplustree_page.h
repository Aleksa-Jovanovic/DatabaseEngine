#pragma once

#include <cstdint>
#include <limits>

namespace db::index {

constexpr std::uint32_t INVALID_PAGE_ID = 
    std::numeric_limits<std::uint32_t>::max();

enum class BPlusTreePageType : std::uint8_t {
    Leaf = 0,
    Internal = 1,
};

struct BPlusTreePageHeader {
    BPlusTreePageType page_type;
    std::uint16_t key_count;
    std::uint16_t max_size;
    std::uint32_t parent_page_id;
};

}  // namespace db::index
