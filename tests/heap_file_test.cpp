#include <cassert>
#include <filesystem>
#include <iostream>

#include "storage/heap/heap_file.h"

// Basic heap-file test:
// write records through HeapFile, then verify lookups still work when
// HeapFile uses the new slotted-page layout internally.
int main() {
    const std::string file_name = "heap_file_test.db";
    std::filesystem::remove(file_name);

    {
        db::HeapFile heap(file_name);

        heap.insert({1, 100});
        heap.insert({2, 200});
        heap.insert({3, 300});

        auto found = heap.find(2);
        assert(found.has_value());
        assert(found->key == 2);
        assert(found->value == 200);

        auto missing = heap.find(999);
        assert(!missing.has_value());
    }

    {
        db::HeapFile reopened_heap(file_name);

        auto found = reopened_heap.find(3);
        assert(found.has_value());
        assert(found->key == 3);
        assert(found->value == 300);
    }

    std::filesystem::remove(file_name);

    std::cout << "HeapFile slotted-page test passed.\n";
    return 0;
}
