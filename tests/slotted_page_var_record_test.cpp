#include <cassert>
#include <iostream>

#include "storage/page/page.h"
#include "storage/page/slotted_page.h"
#include "storage/page/var_record.h"

// Variable-length slotted-page test:
// insert a few VarRecord values with different lengths, read them back,
// and verify deleted variable-length slots become unreadable.
int main() {
    db::Page page{};
    db::SlottedPage slotted_page(page);

    slotted_page.initialize();

    db::VarRecord record1{1, "a"};
    db::VarRecord record2{2, "variable-length value"};
    db::VarRecord record3{3, "this payload is longer than the others"};

    assert(slotted_page.insert_var_record(record1));
    assert(slotted_page.insert_var_record(record2));
    assert(slotted_page.insert_var_record(record3));

    auto read1 = slotted_page.var_record_at(0);
    auto read2 = slotted_page.var_record_at(1);
    auto read3 = slotted_page.var_record_at(2);

    assert(read1.has_value());
    assert(read1->key == 1);
    assert(read1->value == "a");

    assert(read2.has_value());
    assert(read2->key == 2);
    assert(read2->value == "variable-length value");

    assert(read3.has_value());
    assert(read3->key == 3);
    assert(read3->value == "this payload is longer than the others");

    assert(slotted_page.delete_record(1));

    read1 = slotted_page.var_record_at(0);
    read2 = slotted_page.var_record_at(1);
    read3 = slotted_page.var_record_at(2);

    assert(read1.has_value());
    assert(!read2.has_value());
    assert(read3.has_value());

    std::cout << "SlottedPage variable-length record test passed.\n";
    return 0;
}
