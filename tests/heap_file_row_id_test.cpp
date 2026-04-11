#include <cassert>
#include <filesystem>
#include <iostream>

#include "storage/heap/heap_file.h"

// Heap-file RowId test:
// insert fixed-size records, read them back through RowId, then verify
// update and logical delete work across a reopen.
int main() {
    const std::string file_name = "heap_file_row_id_test.db";
    std::filesystem::remove(file_name);

    db::RowId row1{};
    db::RowId row2{};

    {
        db::HeapFile heap(file_name);

        auto inserted1 = heap.insert({1, 100});
        auto inserted2 = heap.insert({2, 200});

        assert(inserted1.has_value());
        assert(inserted2.has_value());

        row1 = *inserted1;
        row2 = *inserted2;

        auto record1 = heap.get(row1);
        auto record2 = heap.get(row2);

        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == 100);

        assert(record2.has_value());
        assert(record2->key == 2);
        assert(record2->value == 200);

        assert(heap.update_record(row1, {1, 999}));

        record1 = heap.get(row1);
        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == 999);

        assert(heap.delete_record(row2));

        record2 = heap.get(row2);
        assert(!record2.has_value());
    }

    {
        db::HeapFile reopened_heap(file_name);

        auto record1 = reopened_heap.get(row1);
        auto record2 = reopened_heap.get(row2);

        assert(record1.has_value());
        assert(record1->key == 1);
        assert(record1->value == 999);

        assert(!record2.has_value());

        auto found = reopened_heap.find(1);
        assert(found.has_value());
        assert(found->value == 999);

        auto missing = reopened_heap.find(2);
        assert(!missing.has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "HeapFile RowId test passed.\n";
    return 0;
}
