#include <cassert>
#include <iostream>

#include "index/bplustree_internal.h"
#include "storage/page/page.h"

int main() {
    db::Page page{};
    db::index::BPlusTreeInternalPage internal_page(page);

    internal_page.initialize(24);

    const db::index::BPlusTreePageHeader* header = internal_page.fetch_header();
    assert(header->page_type == db::index::BPlusTreePageType::Internal);
    assert(header->key_count == 0);
    assert(header->max_size == 24);
    assert(header->parent_page_id == db::index::INVALID_PAGE_ID);
    assert(internal_page.leftmost_child_page_id() == db::index::INVALID_PAGE_ID);
    assert(internal_page.entry_at(0) == nullptr);

    internal_page.set_leftmost_child_page_id(11);
    assert(internal_page.leftmost_child_page_id() == 11);

    internal_page.fetch_header()->key_count = 1;
    db::index::BPlusTreeInternalEntry* first_entry = internal_page.entry_at(0);
    assert(first_entry != nullptr);

    first_entry->key = 50;
    first_entry->right_child_page_id = 13;

    const db::index::BPlusTreeInternalEntry* read_back = internal_page.entry_at(0);
    assert(read_back != nullptr);
    assert(read_back->key == 50);
    assert(read_back->right_child_page_id == 13);
    assert(internal_page.entry_at(1) == nullptr);

    std::cout << "BPlusTreeInternalPage test passed.\n";
    return 0;
}
