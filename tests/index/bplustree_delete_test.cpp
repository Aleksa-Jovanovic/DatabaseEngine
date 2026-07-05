#include <cassert>
#include <filesystem>
#include <iostream>

#include "index/bplustree.h"

int main() {
    const std::string file_name = "bplustree_delete_test.db";
    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);

        assert(!tree.delete_key(10).has_value());

        assert(tree.insert(10, db::RowId{1, 0}).has_value());
        assert(tree.insert(20, db::RowId{2, 1}).has_value());
        assert(tree.insert(30, db::RowId{3, 2}).has_value());

        assert(tree.search(10).has_value());
        assert(tree.search(20).has_value());
        assert(tree.search(30).has_value());

        const auto deleted_20 = tree.delete_key(20);
        assert(deleted_20.has_value());
        assert(deleted_20->page_id == 2);
        assert(deleted_20->slot_index == 1);

        assert(tree.search(10).has_value());
        assert(!tree.search(20).has_value());
        assert(tree.search(30).has_value());

        assert(!tree.delete_key(20).has_value());
        assert(!tree.delete_key(999).has_value());
    }

    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);

        for (std::uint32_t key = 1; key <= 100; ++key) {
            assert(tree.insert(key, db::RowId{key, static_cast<std::uint16_t>(key + 1)}).has_value());
        }

        assert(tree.search(49).has_value());
        assert(tree.search(50).has_value());
        assert(tree.search(51).has_value());

        const auto deleted_50 = tree.delete_key(50);
        assert(deleted_50.has_value());
        assert(deleted_50->page_id == 50);
        assert(deleted_50->slot_index == 51);

        assert(tree.search(49).has_value());
        assert(!tree.search(50).has_value());
        assert(tree.search(51).has_value());

        const auto deleted_1 = tree.delete_key(1);
        assert(deleted_1.has_value());
        assert(!tree.search(1).has_value());
        assert(tree.search(2).has_value());

        const auto deleted_100 = tree.delete_key(100);
        assert(deleted_100.has_value());
        assert(tree.search(99).has_value());
        assert(!tree.search(100).has_value());
    }

    {
        db::index::BPlusTree reopened_tree(file_name, 8);

        assert(!reopened_tree.search(1).has_value());
        assert(reopened_tree.search(2).has_value());
        assert(reopened_tree.search(49).has_value());
        assert(!reopened_tree.search(50).has_value());
        assert(reopened_tree.search(51).has_value());
        assert(reopened_tree.search(99).has_value());
        assert(!reopened_tree.search(100).has_value());

        assert(!reopened_tree.delete_key(50).has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "BPlusTree delete test passed.\n";
    return 0;
}
