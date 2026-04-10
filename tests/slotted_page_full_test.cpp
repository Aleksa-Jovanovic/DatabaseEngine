#include <cassert>
#include <cstdint>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/record.h"
#include "storage/page/slotted_page.h"

// Fill a slotted page until it reports full, then verify all inserted
// records are still readable through the slot directory.
int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    std::uint32_t inserted_count = 0;

    while (true) {
        db::Record record{inserted_count, inserted_count + 1000};

        if (!slotted_page.insert_record(record)) {
            break;
        }

        ++inserted_count;
    }

    assert(inserted_count > 0);
    assert(slotted_page.slot_count() == inserted_count);

    for (std::uint32_t i = 0; i < inserted_count; ++i) {
        auto record = slotted_page.record_at(static_cast<std::uint16_t>(i));
        assert(record.has_value());
        assert(record->key == i);
        assert(record->value == i + 1000);
    }

    db::Record extra_record{999999, 888888};
    assert(!slotted_page.insert_record(extra_record));

    std::cout << "SlottedPage full-page test passed.\n";
    return 0;
}
