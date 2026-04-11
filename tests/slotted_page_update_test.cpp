#include <cassert>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/record.h"
#include "storage/page/slotted_page.h"
#include "storage/page/var_record.h"

// Slotted-page update test:
// verify fixed-size updates overwrite in place, variable-length same-size
// updates overwrite in place, and different-size variable-length updates
// move the updated record to a new slot while deleting the old one.
int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    assert(slotted_page.insert_record({1, 100}));
    assert(slotted_page.update_record(0, {1, 999}));

    auto fixed_record = slotted_page.record_at(0);
    assert(fixed_record.has_value());
    assert(fixed_record->key == 1);
    assert(fixed_record->value == 999);

    assert(slotted_page.insert_var_record({2, "cat"}));
    assert(slotted_page.update_var_record(1, {2, "dog"}));

    auto same_size_var_record = slotted_page.var_record_at(1);
    assert(same_size_var_record.has_value());
    assert(same_size_var_record->key == 2);
    assert(same_size_var_record->value == "dog");

    assert(slotted_page.insert_var_record({3, "tiny"}));
    const std::uint16_t slot_count_before_resize =
        slotted_page.slot_count();

    assert(slotted_page.update_var_record(2, {3, "this is a longer value"}));

    auto old_slot = slotted_page.var_record_at(2);
    assert(!old_slot.has_value());

    assert(slotted_page.slot_count() == slot_count_before_resize + 1);

    auto moved_record = slotted_page.var_record_at(slot_count_before_resize);
    assert(moved_record.has_value());
    assert(moved_record->key == 3);
    assert(moved_record->value == "this is a longer value");

    std::cout << "SlottedPage update test passed.\n";
    return 0;
}
