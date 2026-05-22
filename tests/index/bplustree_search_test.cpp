#include <cassert>
#include <filesystem>
#include <iostream>

#include "common/constants.h"
#include "index/bplustree.h"
#include "index/bplustree_internal.h"
#include "index/bplustree_leaf.h"
#include "storage/cache/page_cache_manager.h"

namespace {

std::uint16_t leaf_node_max_size() {
    constexpr std::size_t header_bytes =
        sizeof(db::index::BPlusTreePageHeader) + sizeof(std::uint32_t);
    return static_cast<std::uint16_t>(
        (db::PAGE_SIZE - header_bytes) / sizeof(db::index::BPlusTreeLeafEntry)
    );
}

std::uint16_t internal_node_max_size() {
    constexpr std::size_t header_bytes =
        sizeof(db::index::BPlusTreePageHeader) + sizeof(std::uint32_t);
    return static_cast<std::uint16_t>(
        (db::PAGE_SIZE - header_bytes) / sizeof(db::index::BPlusTreeInternalEntry)
    );
}

}  // namespace

int main() {
    const std::string file_name = "bplustree_search_test.db";
    std::filesystem::remove(file_name);

    {
        db::index::BPlusTree tree(file_name, 8);
        auto result = tree.search(42);
        assert(!result.has_value());
    }

    std::filesystem::remove(file_name);

    {
        std::uint32_t root_page_id = db::index::INVALID_PAGE_ID;

        {
            db::index::BPlusTree bootstrap_tree(file_name, 8);
        }

        {
            db::PageCacheManager cache(file_name, 8);

            db::Page* root_page = cache.new_page();
            assert(root_page != nullptr);

            root_page_id = root_page->page_id;
            db::index::BPlusTreeLeafPage leaf_page(*root_page);
            leaf_page.initialize(leaf_node_max_size());

            assert(leaf_page.insert_entry(10, db::RowId{1, 0}));
            assert(leaf_page.insert_entry(20, db::RowId{2, 1}));
            assert(leaf_page.insert_entry(30, db::RowId{3, 2}));

            assert(cache.unpin_page(root_page_id, true));
            cache.flush_all_pages();
        }

        db::index::BPlusTree tree(file_name, 8);
        tree.set_root_page_id(root_page_id);

        auto found_10 = tree.search(10);
        auto found_20 = tree.search(20);
        auto found_30 = tree.search(30);
        auto missing = tree.search(999);

        assert(found_10.has_value());
        assert(found_10->page_id == 1);
        assert(found_10->slot_index == 0);

        assert(found_20.has_value());
        assert(found_20->page_id == 2);
        assert(found_20->slot_index == 1);

        assert(found_30.has_value());
        assert(found_30->page_id == 3);
        assert(found_30->slot_index == 2);

        assert(!missing.has_value());
    }

    std::filesystem::remove(file_name);

    {
        std::uint32_t left_leaf_page_id = db::index::INVALID_PAGE_ID;
        std::uint32_t right_leaf_page_id = db::index::INVALID_PAGE_ID;
        std::uint32_t root_page_id = db::index::INVALID_PAGE_ID;

        {
            db::index::BPlusTree bootstrap_tree(file_name, 8);
        }

        {
            db::PageCacheManager cache(file_name, 8);

            db::Page* left_leaf_page = cache.new_page();
            assert(left_leaf_page != nullptr);
            left_leaf_page_id = left_leaf_page->page_id;

            db::index::BPlusTreeLeafPage left_leaf(*left_leaf_page);
            left_leaf.initialize(leaf_node_max_size());
            assert(left_leaf.insert_entry(10, db::RowId{1, 0}));
            assert(left_leaf.insert_entry(20, db::RowId{2, 1}));
            assert(cache.unpin_page(left_leaf_page_id, true));

            db::Page* right_leaf_page = cache.new_page();
            assert(right_leaf_page != nullptr);
            right_leaf_page_id = right_leaf_page->page_id;

            db::index::BPlusTreeLeafPage right_leaf(*right_leaf_page);
            right_leaf.initialize(leaf_node_max_size());
            assert(right_leaf.insert_entry(30, db::RowId{3, 2}));
            assert(right_leaf.insert_entry(40, db::RowId{4, 3}));
            assert(cache.unpin_page(right_leaf_page_id, true));

            db::Page* root_page = cache.new_page();
            assert(root_page != nullptr);
            root_page_id = root_page->page_id;

            db::index::BPlusTreeInternalPage root_internal(*root_page);
            root_internal.initialize(internal_node_max_size());
            root_internal.set_leftmost_child_page_id(left_leaf_page_id);
            assert(root_internal.insert_after_child(left_leaf_page_id, 30, right_leaf_page_id));
            assert(cache.unpin_page(root_page_id, true));

            cache.flush_all_pages();
        }

        db::index::BPlusTree tree(file_name, 8);
        tree.set_root_page_id(root_page_id);

        auto found_10 = tree.search(10);
        auto found_20 = tree.search(20);
        auto found_30 = tree.search(30);
        auto found_40 = tree.search(40);
        auto missing = tree.search(999);

        assert(found_10.has_value());
        assert(found_10->page_id == 1);
        assert(found_10->slot_index == 0);

        assert(found_20.has_value());
        assert(found_20->page_id == 2);
        assert(found_20->slot_index == 1);

        assert(found_30.has_value());
        assert(found_30->page_id == 3);
        assert(found_30->slot_index == 2);

        assert(found_40.has_value());
        assert(found_40->page_id == 4);
        assert(found_40->slot_index == 3);

        assert(!missing.has_value());
    }

    std::filesystem::remove(file_name);

    std::cout << "BPlusTree search test passed.\n";
    return 0;
}
