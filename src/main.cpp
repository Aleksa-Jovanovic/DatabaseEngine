#include <iostream>
#include "storage/heap/heap_file.h"

int main() {
    db::HeapFile heap("test.db");

    heap.insert({1, 100});
    heap.insert({2, 200});
    heap.insert({3, 300});

    auto result = heap.find(3);
    if (result.has_value()) {
        std::cout << "Found key=" << result->key << " value=" << result->value << '\n';
    } else {
        std::cout << "Not found\n";
    }

    return 0;
}