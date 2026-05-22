#include <cassert>
#include <iostream>

#include "index/bplustree_leaf.h"
#include "storage/page/page.h"

int main() {
    db::Page page{};
    db::index::BPlusTreeLeafPage leaf_page(page);

    leaf_page.initialize(3);

    const db::index::BPlusTreePageHeader* header = leaf_page.fetch_header();
    assert(header->page_type == db::index::BPlusTreePageType::Leaf);
    assert(header->key_count == 0);
    assert(header->max_size == 3);
    assert(header->parent_page_id == db::index::INVALID_PAGE_ID);
    assert(leaf_page.next_leaf_page_id() == db::index::INVALID_PAGE_ID);
    assert(leaf_page.entry_at(0) == nullptr);
    assert(leaf_page.find_entry(42) == nullptr);

    leaf_page.set_next_leaf_page_id(19);
    assert(leaf_page.next_leaf_page_id() == 19);

    assert(leaf_page.insert_entry(20, db::RowId{2, 0}));
    assert(leaf_page.insert_entry(10, db::RowId{1, 1}));
    assert(leaf_page.insert_entry(30, db::RowId{3, 2}));

    assert(leaf_page.key_count() == 3);
    assert(leaf_page.is_full());

    const db::index::BPlusTreeLeafEntry* first_entry = leaf_page.entry_at(0);
    const db::index::BPlusTreeLeafEntry* second_entry = leaf_page.entry_at(1);
    const db::index::BPlusTreeLeafEntry* third_entry = leaf_page.entry_at(2);

    assert(first_entry != nullptr);
    assert(second_entry != nullptr);
    assert(third_entry != nullptr);

    assert(first_entry->key == 10);
    assert(first_entry->row_id.page_id == 1);
    assert(first_entry->row_id.slot_index == 1);

    assert(second_entry->key == 20);
    assert(second_entry->row_id.page_id == 2);
    assert(second_entry->row_id.slot_index == 0);

    assert(third_entry->key == 30);
    assert(third_entry->row_id.page_id == 3);
    assert(third_entry->row_id.slot_index == 2);

    const db::index::BPlusTreeLeafEntry* found_10 = leaf_page.find_entry(10);
    const db::index::BPlusTreeLeafEntry* found_20 = leaf_page.find_entry(20);
    const db::index::BPlusTreeLeafEntry* found_30 = leaf_page.find_entry(30);

    assert(found_10 != nullptr);
    assert(found_20 != nullptr);
    assert(found_30 != nullptr);
    assert(found_10->row_id.page_id == 1);
    assert(found_20->row_id.page_id == 2);
    assert(found_30->row_id.page_id == 3);

    assert(leaf_page.find_entry(999) == nullptr);
    assert(!leaf_page.insert_entry(20, db::RowId{9, 9}));
    assert(!leaf_page.insert_entry(40, db::RowId{4, 4}));
    assert(leaf_page.entry_at(3) == nullptr);

    std::cout << "BPlusTreeLeafPage test passed.\n";
    return 0;
}
