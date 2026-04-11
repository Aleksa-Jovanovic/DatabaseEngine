#include <iostream>
#include "storage/heap/heap_file.h"

int main() {
    db::HeapFile heap("test.db");

    auto row_id = heap.insert({1, 100});
    row_id = heap.insert({2, 200});
    row_id = heap.insert({3, 300});

    auto result = heap.find(3);
    if (result.has_value()) {
        std::cout << "Found key=" << result->key << " value=" << result->value << '\n';
    } else {
        std::cout << "Not found\n";
    }

    return 0;
}