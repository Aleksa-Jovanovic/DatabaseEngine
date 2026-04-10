#include <cassert>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/page_layout.h"
#include "storage/page/record.h"
#include "storage/page/slotted_page.h"

// Basic slotted-page smoke test:
// initialize an empty page, insert a few fixed-size records,
// verify slot-based reads, and check free-space accounting.

int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    // Check initial header state.
    const db::SlottedPageHeader* header = slotted_page.fetch_header();
    assert(header->slot_count == 0);
    assert(header->free_space_start == sizeof(db::SlottedPageHeader));
    assert(header->free_space_end == db::PAGE_SIZE);
    assert(slotted_page.free_space() == db::PAGE_SIZE - sizeof(db::SlottedPageHeader));

    // Insert the first record.
    db::Record record1{1, 100};
    const bool inserted1 = slotted_page.insert_record(record1);
    assert(inserted1);

    assert(slotted_page.slot_count() == 1);

    auto read_back1 = slotted_page.record_at(0);
    assert(read_back1.has_value());
    assert(read_back1->key == 1);
    assert(read_back1->value == 100);

    // Insert a second record.
    db::Record record2{2, 200};
    const bool inserted2 = slotted_page.insert_record(record2);
    assert(inserted2);

    assert(slotted_page.slot_count() == 2);

    auto read_back2 = slotted_page.record_at(1);
    assert(read_back2.has_value());
    assert(read_back2->key == 2);
    assert(read_back2->value == 200);

    // Check that the first record is still intact.
    read_back1 = slotted_page.record_at(0);
    assert(read_back1.has_value());
    assert(read_back1->key == 1);
    assert(read_back1->value == 100);

    // Verify free space shrinks as expected.
    const std::size_t expected_free_space =
        db::PAGE_SIZE
        - sizeof(db::SlottedPageHeader)
        - 2 * sizeof(db::SlotEntry)
        - 2 * sizeof(db::Record);

    assert(slotted_page.free_space() == expected_free_space);

    std::cout << "SlottedPage test passed.\n";
    return 0;
}
