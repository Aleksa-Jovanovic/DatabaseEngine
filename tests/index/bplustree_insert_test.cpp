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

        // Insert into the existing right leaf until it becomes full.
        auto fill_right_leaf = tree.insert(35, db::RowId{6, 5});
        assert(fill_right_leaf.has_value());
        assert(fill_right_leaf->page_id == 6);
        assert(fill_right_leaf->slot_index == 5);

        auto found_35 = tree.search(35);
        assert(found_35.has_value());
        assert(found_35->page_id == 6);
        assert(found_35->slot_index == 5);

        // This insert should split a non-root leaf and add a separator to the
        // existing internal root, which still has free space.
        auto non_root_split_insert = tree.insert(45, db::RowId{7, 6});
        assert(non_root_split_insert.has_value());
        assert(non_root_split_insert->page_id == 7);
        assert(non_root_split_insert->slot_index == 6);

        found_10 = tree.search(10);
        found_20 = tree.search(20);
        found_25 = tree.search(25);
        found_30 = tree.search(30);
        found_35 = tree.search(35);
        found_40 = tree.search(40);
        auto found_45 = tree.search(45);
        auto missing_after_non_root_split = tree.search(1000);

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

        assert(found_35.has_value());
        assert(found_35->page_id == 6);
        assert(found_35->slot_index == 5);

        assert(found_40.has_value());
        assert(found_40->page_id == 4);
        assert(found_40->slot_index == 3);

        assert(found_45.has_value());
        assert(found_45->page_id == 7);
        assert(found_45->slot_index == 6);

        assert(!missing_after_non_root_split.has_value());

        // Grow the right side again so the internal root collects more
        // separator keys.
        auto fill_far_right_leaf = tree.insert(50, db::RowId{8, 7});
        assert(fill_far_right_leaf.has_value());
        assert(fill_far_right_leaf->page_id == 8);
        assert(fill_far_right_leaf->slot_index == 7);

        auto split_far_right_leaf = tree.insert(55, db::RowId{9, 8});
        assert(split_far_right_leaf.has_value());
        assert(split_far_right_leaf->page_id == 9);
        assert(split_far_right_leaf->slot_index == 8);

        // Split the left side once so the root internal page reaches its key
        // capacity before the next propagated insert.
        auto grow_left_leaf = tree.insert(15, db::RowId{10, 9});
        assert(grow_left_leaf.has_value());
        assert(grow_left_leaf->page_id == 10);
        assert(grow_left_leaf->slot_index == 9);

        auto fill_left_leaf = tree.insert(12, db::RowId{11, 10});
        assert(fill_left_leaf.has_value());
        assert(fill_left_leaf->page_id == 11);
        assert(fill_left_leaf->slot_index == 10);

        auto split_left_leaf = tree.insert(11, db::RowId{12, 11});
        assert(split_left_leaf.has_value());
        assert(split_left_leaf->page_id == 12);
        assert(split_left_leaf->slot_index == 11);

        // The root internal page should now be full. This next leaf split must
        // split that internal root and create a taller tree.
        auto fill_new_far_right_leaf = tree.insert(60, db::RowId{13, 12});
        assert(fill_new_far_right_leaf.has_value());
        assert(fill_new_far_right_leaf->page_id == 13);
        assert(fill_new_far_right_leaf->slot_index == 12);

        auto recursive_internal_split = tree.insert(65, db::RowId{14, 13});
        assert(recursive_internal_split.has_value());
        assert(recursive_internal_split->page_id == 14);
        assert(recursive_internal_split->slot_index == 13);

        auto found_11 = tree.search(11);
        auto found_12 = tree.search(12);
        auto found_15 = tree.search(15);
        found_25 = tree.search(25);
        found_35 = tree.search(35);
        auto found_50 = tree.search(50);
        auto found_55 = tree.search(55);
        auto found_60 = tree.search(60);
        auto found_65 = tree.search(65);
        auto missing_after_internal_split = tree.search(5000);

        assert(found_11.has_value());
        assert(found_11->page_id == 12);
        assert(found_11->slot_index == 11);

        assert(found_12.has_value());
        assert(found_12->page_id == 11);
        assert(found_12->slot_index == 10);

        assert(found_15.has_value());
        assert(found_15->page_id == 10);
        assert(found_15->slot_index == 9);

        assert(found_25.has_value());
        assert(found_25->page_id == 5);
        assert(found_25->slot_index == 4);

        assert(found_35.has_value());
        assert(found_35->page_id == 6);
        assert(found_35->slot_index == 5);

        assert(found_50.has_value());
        assert(found_50->page_id == 8);
        assert(found_50->slot_index == 7);

        assert(found_55.has_value());
        assert(found_55->page_id == 9);
        assert(found_55->slot_index == 8);

        assert(found_60.has_value());
        assert(found_60->page_id == 13);
        assert(found_60->slot_index == 12);

        assert(found_65.has_value());
        assert(found_65->page_id == 14);
        assert(found_65->slot_index == 13);

        assert(!missing_after_internal_split.has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "BPlusTree insert test passed.\n";
    return 0;
}
