#include <cassert>
#include <filesystem>
#include <iostream>

#include "storage/heap/heap_file.h"

// Heap-file variable-length record test:
// insert variable-length records, read them back through RowId, verify
// same-size and size-changing updates, then confirm delete and reopen behavior.
int main() {
    const std::string file_name = "heap_file_var_record_test.db";
    std::filesystem::remove(file_name);

    db::RowId row1{};
    db::RowId row2{};

    {
        db::HeapFile heap(file_name);

        auto inserted1 = heap.insert_var_record({1, "cat"});
        auto inserted2 = heap.insert_var_record({2, "short"});

        assert(inserted1.has_value());
        assert(inserted2.has_value());

        row1 = *inserted1;
        row2 = *inserted2;

        auto record1 = heap.get_var_record(row1);
        auto record2 = heap.get_var_record(row2);

        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == "cat");

        assert(record2.has_value());
        assert(record2->key == 2);
        assert(record2->value == "short");

        // Same-size update should preserve RowId.
        auto updated_same_size = heap.update_var_record(row1, {1, "dog"});
        assert(updated_same_size.has_value());
        assert(updated_same_size->page_id == row1.page_id);
        assert(updated_same_size->slot_index == row1.slot_index);

        record1 = heap.get_var_record(row1);
        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == "dog");

        // Size-changing update may move the record to a new slot.
        auto updated_resized = heap.update_var_record(row2, {2, "this is a much longer value"});
        assert(updated_resized.has_value());

        auto old_location = heap.get_var_record(row2);
        assert(!old_location.has_value());

        row2 = *updated_resized;

        record2 = heap.get_var_record(row2);
        assert(record2.has_value());
        assert(record2->key == 2);
        assert(record2->value == "this is a much longer value");

        assert(heap.delete_record(row2));
        record2 = heap.get_var_record(row2);
        assert(!record2.has_value());
    }

    {
        db::HeapFile reopened_heap(file_name);

        auto record1 = reopened_heap.get_var_record(row1);
        auto record2 = reopened_heap.get_var_record(row2);

        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == "dog");

        assert(!record2.has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "HeapFile variable-length record test passed.\n";
    return 0;
}
