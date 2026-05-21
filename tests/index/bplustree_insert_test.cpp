#include <cassert>
#include <filesystem>
#include <iostream>

#include "index/bplustree.h"

int main() {
    const std::string file_name = "bplustree_insert_test.db";
    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);

        auto missing_before_insert = tree.search(42);
        assert(!missing_before_insert.has_value());

        assert(tree.insert(20, db::RowId{2, 0}).has_value());
        assert(tree.insert(10, db::RowId{1, 1}).has_value());
        assert(tree.insert(30, db::RowId{3, 2}).has_value());
        assert(tree.insert(40, db::RowId{4, 3}).has_value());

        auto duplicate = tree.insert(20, db::RowId{9, 9});
        assert(!duplicate.has_value());

        auto found_10 = tree.search(10);
        auto found_20 = tree.search(20);
        auto found_30 = tree.search(30);
        auto found_40 = tree.search(40);

        assert(found_10.has_value());
        assert(found_10->page_id == 1);
        assert(found_10->slot_index == 1);

        assert(found_20.has_value());
        assert(found_20->page_id == 2);
        assert(found_20->slot_index == 0);

        assert(found_30.has_value());
        assert(found_30->page_id == 3);
        assert(found_30->slot_index == 2);

        assert(found_40.has_value());
        assert(found_40->page_id == 4);
        assert(found_40->slot_index == 3);

        // Fifth insert should force a split of the full root leaf and create
        // a new internal root.
        auto split_insert = tree.insert(25, db::RowId{5, 4});
        assert(split_insert.has_value());
        assert(split_insert->page_id == 5);
        assert(split_insert->slot_index == 4);

        found_10 = tree.search(10);
        found_20 = tree.search(20);
        auto found_25 = tree.search(25);
        found_30 = tree.search(30);
        found_40 = tree.search(40);
        auto missing_after_split = tree.search(999);

        assert(found_10.has_value());
        assert(found_10->page_id == 1);
        assert(found_10->slot_index == 1);

        assert(found_20.has_value());
        assert(found_20->page_id == 2);
        assert(found_20->slot_index == 0);

        assert(found_25.has_value());
        assert(found_25->page_id == 5);
        assert(found_25->slot_index == 4);

        assert(found_30.has_value());
        assert(found_30->page_id == 3);
        assert(found_30->slot_index == 2);

        assert(found_40.has_value());
        assert(found_40->page_id == 4);
        assert(found_40->slot_index == 3);

        assert(!missing_after_split.has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "BPlusTree insert test passed.\n";
    return 0;
}
