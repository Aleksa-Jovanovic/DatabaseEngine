#include <cassert>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/slotted_page.h"
#include "storage/page/var_record.h"

// Slotted-page compaction test:
// create fragmentation with delete and relocation, compact the page, then
// verify live records remain readable and free space increases.
int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    assert(slotted_page.insert_var_record({1, "alpha"}));
    assert(slotted_page.insert_var_record({2, "beta"}));
    assert(slotted_page.insert_var_record({3, "this record is significantly longer"}));

    const std::size_t free_space_before_delete = slotted_page.free_space();

    // Create a hole by deleting the middle slot.
    assert(slotted_page.delete_record(1));

    const std::size_t free_space_after_delete = slotted_page.free_space();
    assert(free_space_after_delete == free_space_before_delete);

    // Create another hole by updating slot 0 with a larger value that moves it.
    assert(slotted_page.update_var_record(0, {1, "this updated value is much longer than alpha"}));

    const std::uint16_t moved_slot_index =
        static_cast<std::uint16_t>(slotted_page.slot_count() - 1);

    auto deleted_old_slot = slotted_page.var_record_at(0);
    auto deleted_middle_slot = slotted_page.var_record_at(1);
    auto surviving_record = slotted_page.var_record_at(2);
    auto moved_record = slotted_page.var_record_at(moved_slot_index);

    assert(!deleted_old_slot.has_value());
    assert(!deleted_middle_slot.has_value());

    assert(surviving_record.has_value());
    assert(surviving_record->key == 3);
    assert(surviving_record->value == "this record is significantly longer");

    assert(moved_record.has_value());
    assert(moved_record->key == 1);
    assert(moved_record->value == "this updated value is much longer than alpha");

    const std::size_t free_space_before_compaction = slotted_page.free_space();

    slotted_page.compact_page();

    const std::size_t free_space_after_compaction = slotted_page.free_space();
    assert(free_space_after_compaction > free_space_before_compaction);

    deleted_old_slot = slotted_page.var_record_at(0);
    deleted_middle_slot = slotted_page.var_record_at(1);
    surviving_record = slotted_page.var_record_at(2);
    moved_record = slotted_page.var_record_at(moved_slot_index);

    assert(!deleted_old_slot.has_value());
    assert(!deleted_middle_slot.has_value());

    assert(surviving_record.has_value());
    assert(surviving_record->key == 3);
    assert(surviving_record->value == "this record is significantly longer");

    assert(moved_record.has_value());
    assert(moved_record->key == 1);
    assert(moved_record->value == "this updated value is much longer than alpha");

    std::cout << "SlottedPage compaction test passed.\n";
    return 0;
}
