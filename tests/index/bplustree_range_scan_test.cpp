#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

#include "index/bplustree.h"

namespace {

void assert_row_ids_match_keys(
    const std::vector<db::RowId>& row_ids,
    const std::vector<std::uint32_t>& expected_keys
) {
    assert(row_ids.size() == expected_keys.size());

    for (std::size_t i = 0; i < expected_keys.size(); ++i) {
        assert(row_ids[i].page_id == expected_keys[i]);
        assert(row_ids[i].slot_index == static_cast<std::uint16_t>(expected_keys[i] + 1));
    }
}

}  // namespace

int main() {
    const std::string file_name = "bplustree_range_scan_test.db";
    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);

        const auto empty_tree_result = tree.range_scan(1, 10);
        assert(empty_tree_result.empty());

        const auto invalid_range_result = tree.range_scan(10, 1);
        assert(invalid_range_result.empty());
    }

    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);

        for (std::uint32_t key = 1; key <= 100; ++key) {
            assert(tree.insert(
                key,
                db::RowId{key, static_cast<std::uint16_t>(key + 1)}
            ).has_value());
        }

        assert_row_ids_match_keys(
            tree.range_scan(10, 15),
            {10, 11, 12, 13, 14, 15}
        );

        assert_row_ids_match_keys(
            tree.range_scan(1, 3),
            {1, 2, 3}
        );

        assert_row_ids_match_keys(
            tree.range_scan(98, 100),
            {98, 99, 100}
        );

        assert_row_ids_match_keys(
            tree.range_scan(0, 2),
            {1, 2}
        );

        assert_row_ids_match_keys(
            tree.range_scan(99, 150),
            {99, 100}
        );

        assert(tree.range_scan(101, 150).empty());
        assert(tree.range_scan(55, 54).empty());
    }

    {
        db::index::BPlusTree reopened_tree(file_name, 8);

        assert_row_ids_match_keys(
            reopened_tree.range_scan(45, 55),
            {45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55}
        );

        assert(reopened_tree.range_scan(101, 200).empty());
    }

    std::filesystem::remove(file_name);

    std::cout << "BPlusTree range scan test passed.\n";
    return 0;
}
