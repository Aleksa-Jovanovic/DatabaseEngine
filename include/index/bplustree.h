#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "common/row_id.h"
#include "index/bplustree_page.h"
#include "storage/cache/page_cache_manager.h"

namespace db::index {

// This index uses a B+ tree layout so all searchable keys live in the leaf
// pages, while internal pages only route traversal. That keeps the leaf level
// linked through `next_leaf_page_id`, which later enables faster range scans.
class BPlusTree {
public:
    BPlusTree(const std::string& file_name, std::size_t cache_size = 8);

    std::optional<RowId> search(std::uint32_t key);
    std::optional<RowId> insert(std::uint32_t key, const RowId& row_id);

#ifdef DB_TESTING
    void set_root_page_id(std::uint32_t page_id) {
        root_page_id_ = page_id;
    }
#endif

private:
    // Carries the separator and page ids produced by an internal-node split.
    struct InternalInsertResult {
        std::uint32_t promoted_key;
        std::uint32_t left_page_id;
        std::uint32_t right_page_id;
    };
    
    PageCacheManager page_cache_manager_;
    std::uint32_t root_page_id_ = INVALID_PAGE_ID;

    // Create the first leaf root for an empty tree.
    std::optional<RowId> create_initial_tree(std::uint32_t key, const RowId& row_id);

    // Traverse from the root to the leaf where this key belongs.
    std::optional<std::uint32_t> find_leaf_page_id(std::uint32_t key);

    // Insert directly into a leaf that still has space.
    std::optional<RowId> insert_into_leaf(std::uint32_t leaf_page_id, std::uint32_t key, const RowId& row_id);

    // Split a full leaf, then push the separator into its parent.
    std::optional<RowId> split_leaf_and_insert(std::uint32_t leaf_page_id, std::uint32_t key, const RowId& row_id);
    
    // Insert a promoted separator into a parent, splitting upward if needed.
    std::optional<RowId> insert_into_parent(
        std::uint32_t parent_page_id,
        std::uint32_t left_child_page_id,
        std::uint32_t key,
        std::uint32_t right_child_page_id,
        const RowId& row_id
    );

    // Split a full internal node and return the separator promoted upward.
    std::optional<InternalInsertResult> split_internal_and_insert(
        std::uint32_t internal_page_id,
        std::uint32_t left_child_page_id,
        std::uint32_t key,
        std::uint32_t right_child_page_id
    );

    // Build a new root above two child pages after a split reaches the top.
    std::optional<RowId> create_new_root(
        std::uint32_t left_child_page_id,
        std::uint32_t key,
        std::uint32_t right_child_page_id,
        const RowId& row_id
    );
};

}  // namespace db::index
