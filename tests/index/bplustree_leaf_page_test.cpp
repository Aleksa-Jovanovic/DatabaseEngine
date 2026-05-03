#include <cassert>
#include <iostream>

#include "index/bplustree_leaf.h"
#include "storage/page/page.h"

int main() {
    db::Page page{};
    db::index::BPlusTreeLeafPage leaf_page(page);

    leaf_page.initialize(32);

    const db::index::BPlusTreePageHeader* header = leaf_page.fetch_header();
    assert(header->page_type == db::index::BPlusTreePageType::Leaf);
    assert(header->key_count == 0);
    assert(header->max_size == 32);
    assert(header->parent_page_id == db::index::INVALID_PAGE_ID);
    assert(leaf_page.next_leaf_page_id() == db::index::INVALID_PAGE_ID);
    assert(leaf_page.entry_at(0) == nullptr);

    leaf_page.set_next_leaf_page_id(19);
    assert(leaf_page.next_leaf_page_id() == 19);

    leaf_page.fetch_header()->key_count = 1;
    db::index::BPlusTreeLeafEntry* first_entry = leaf_page.entry_at(0);
    assert(first_entry != nullptr);

    first_entry->key = 42;
    first_entry->row_id = db::RowId{7, 3};

    const db::index::BPlusTreeLeafEntry* read_back = leaf_page.entry_at(0);
    assert(read_back != nullptr);
    assert(read_back->key == 42);
    assert(read_back->row_id.page_id == 7);
    assert(read_back->row_id.slot_index == 3);
    assert(leaf_page.entry_at(1) == nullptr);

    std::cout << "BPlusTreeLeafPage test passed.\n";
    return 0;
}
