#pragma once

#include <cstdint>

#include "common/row_id.h"
#include "index/bplustree_page.h"
#include "storage/page/page.h"

namespace db::index {

struct BPlusTreeLeafEntry {
    std::uint32_t key;
    RowId row_id;
};

class BPlusTreeLeafPage {
public:
    explicit BPlusTreeLeafPage(Page& page);

    void initialize(std::uint16_t max_size);

    BPlusTreePageHeader* fetch_header();
    const BPlusTreePageHeader* fetch_header() const;

    std::uint32_t next_leaf_page_id() const;
    void set_next_leaf_page_id(std::uint32_t page_id);

    std::uint16_t key_count() const;
    std::uint16_t max_size() const;
    bool is_full() const;

    const BPlusTreeLeafEntry* find_entry(std::uint32_t key) const;
    bool insert_entry(std::uint32_t key, const RowId& row_id);
    bool delete_entry(std::uint32_t key);

    BPlusTreeLeafEntry* entry_at(std::uint16_t index);
    const BPlusTreeLeafEntry* entry_at(std::uint16_t index) const;

private:
    // | BPlusTreePageHeader | next_leaf_page_id | BPlusTreeLeafEntry[] |
    Page& page_;

    // Return a pointer to the start of the raw page bytes.
    char* data();
    const char* data() const;

    std::uint32_t* next_leaf_page_id_ptr();
    const std::uint32_t* next_leaf_page_id_ptr() const;

    BPlusTreeLeafEntry* entries();
    const BPlusTreeLeafEntry* entries() const;

    std::int32_t find_key_index(std::uint32_t key) const;
    std::uint16_t find_insert_position(std::uint32_t key) const;
};

}  // namespace db::index
