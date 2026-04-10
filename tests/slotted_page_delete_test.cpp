#include <cassert>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/record.h"
#include "storage/page/slotted_page.h"

// Deletion test for slotted pages:
// insert a few records, delete one slot, verify the deleted record is no
// longer readable, and verify repeated delete fails.
int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    assert(slotted_page.insert_record({1, 100}));
    assert(slotted_page.insert_record({2, 200}));
    assert(slotted_page.insert_record({3, 300}));

    auto first = slotted_page.record_at(0);
    auto second = slotted_page.record_at(1);
    auto third = slotted_page.record_at(2);

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());

    assert(slotted_page.delete_record(1));

    first = slotted_page.record_at(0);
    second = slotted_page.record_at(1);
    third = slotted_page.record_at(2);

    assert(first.has_value());
    assert(first->key == 1);
    assert(first->value == 100);

    assert(!second.has_value());

    assert(third.has_value());
    assert(third->key == 3);
    assert(third->value == 300);

    assert(!slotted_page.delete_record(1));
    assert(!slotted_page.delete_record(99));

    std::cout << "SlottedPage delete test passed.\n";
    return 0;
}
