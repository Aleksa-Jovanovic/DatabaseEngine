#pragma once

#include <cstdint>

#include "index/bplustree_page.h"
#include "storage/page/page.h"

namespace db::index {

struct BPlusTreeInternalEntry {
    std::uint32_t key;
    std::uint32_t right_child_page_id;
};

class BPlusTreeInternalPage {
public:
    explicit BPlusTreeInternalPage(Page& page);

    void initialize(std::uint16_t max_size);

    BPlusTreePageHeader* fetch_header();
    const BPlusTreePageHeader* fetch_header() const;

    std::uint32_t leftmost_child_page_id() const;
    void set_leftmost_child_page_id(std::uint32_t page_id);

    std::uint16_t key_count() const;
    std::uint16_t max_size() const;
    bool is_full() const;

    BPlusTreeInternalEntry* entry_at(std::uint16_t index);
    const BPlusTreeInternalEntry* entry_at(std::uint16_t index) const;

    std::uint32_t find_child_page_id(std::uint32_t key) const;
    //bool insert_entry(std::uint32_t key, std::uint32_t right_child_page_id);
    bool insert_after_child(
        std::uint32_t left_child_page_id,
        std::uint32_t key,
        std::uint32_t right_child_page_id
    );

private:
    // | BPlusTreePageHeader | leftmost_child_page_id | BPlusTreeInternalEntry[] |
    Page& page_;

    // Return a pointer to the start of the raw page bytes.
    char* data();
    const char* data() const;

    std::uint32_t* leftmost_child_page_id_ptr();
    const std::uint32_t* leftmost_child_page_id_ptr() const;

    BPlusTreeInternalEntry* entries();
    const BPlusTreeInternalEntry* entries() const;

    std::int32_t find_key_index(std::uint32_t key) const;
    //std::uint16_t find_insert_position(std::uint32_t key) const;
    std::int32_t find_child_index(std::uint32_t child_page_id) const;
};

}  // namespace db::index
