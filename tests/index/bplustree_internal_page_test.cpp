#include <cassert>
#include <iostream>

#include "index/bplustree_internal.h"
#include "storage/page/page.h"

int main() {
    db::Page page{};
    db::index::BPlusTreeInternalPage internal_page(page);

    internal_page.initialize(3);

    const db::index::BPlusTreePageHeader* header = internal_page.fetch_header();
    assert(header->page_type == db::index::BPlusTreePageType::Internal);
    assert(header->key_count == 0);
    assert(header->max_size == 3);
    assert(header->parent_page_id == db::index::INVALID_PAGE_ID);
    assert(internal_page.leftmost_child_page_id() == db::index::INVALID_PAGE_ID);
    assert(internal_page.entry_at(0) == nullptr);

    internal_page.set_leftmost_child_page_id(5);
    assert(internal_page.leftmost_child_page_id() == 5);
    assert(internal_page.find_child_page_id(1) == 5);
    assert(internal_page.find_child_page_id(999) == 5);

    assert(internal_page.insert_after_child(5, 20, 8));
    assert(internal_page.insert_after_child(5, 10, 6));
    assert(internal_page.insert_after_child(8, 30, 9));

    assert(internal_page.key_count() == 3);
    assert(internal_page.is_full());

    const db::index::BPlusTreeInternalEntry* first_entry = internal_page.entry_at(0);
    const db::index::BPlusTreeInternalEntry* second_entry = internal_page.entry_at(1);
    const db::index::BPlusTreeInternalEntry* third_entry = internal_page.entry_at(2);

    assert(first_entry != nullptr);
    assert(second_entry != nullptr);
    assert(third_entry != nullptr);

    assert(first_entry->key == 10);
    assert(first_entry->right_child_page_id == 6);

    assert(second_entry->key == 20);
    assert(second_entry->right_child_page_id == 8);

    assert(third_entry->key == 30);
    assert(third_entry->right_child_page_id == 9);

    assert(internal_page.find_child_page_id(5) == 5);
    assert(internal_page.find_child_page_id(10) == 6);
    assert(internal_page.find_child_page_id(15) == 6);
    assert(internal_page.find_child_page_id(20) == 8);
    assert(internal_page.find_child_page_id(25) == 8);
    assert(internal_page.find_child_page_id(30) == 9);
    assert(internal_page.find_child_page_id(35) == 9);

    assert(!internal_page.insert_after_child(5, 20, 99));
    assert(!internal_page.insert_after_child(9, 40, 10));
    assert(internal_page.entry_at(3) == nullptr);

    std::cout << "BPlusTreeInternalPage test passed.\n";
    return 0;
}
