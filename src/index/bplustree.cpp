#include "index/bplustree.h"

#include <algorithm>
#include <vector>

#include "index/bplustree_internal.h"
#include "index/bplustree_leaf.h"
#include "storage/page/page.h"

namespace db::index {

// Keep this helper local to this translation unit because it is only used
// while rebuilding a split root leaf into two leaf pages.
namespace {
struct TempLeafEntry {
    std::uint32_t key;
    RowId row_id;
};
}

BPlusTree::BPlusTree(const std::string& file_name, std::size_t cache_size)
    : page_cache_manager_(file_name, cache_size) {}

std::optional<RowId> BPlusTree::search(std::uint32_t key) {
    if (root_page_id_ == INVALID_PAGE_ID) {
        return std::nullopt;
    }

    std::uint32_t current_page_id = root_page_id_;

    while (true) {
        Page* page_ptr = page_cache_manager_.fetch_page(current_page_id);
        if (page_ptr == nullptr) {
            return std::nullopt;
        }

        const BPlusTreePageHeader* header = 
            reinterpret_cast<const BPlusTreePageHeader*>(page_ptr->data.data());
        
        if (header->page_type == BPlusTreePageType::Leaf) {
            BPlusTreeLeafPage leaf_page(*page_ptr);
            const BPlusTreeLeafEntry* entry = leaf_page.find_entry(key);

            std::optional<RowId> result = std::nullopt;
            if (entry != nullptr) {
                result = entry->row_id;
            }

            page_cache_manager_.unpin_page(current_page_id, false);
            return result;
        }

        if (header->page_type == BPlusTreePageType::Internal) {
            BPlusTreeInternalPage internal_page(*page_ptr);
            const std::uint32_t next_page_id = internal_page.find_child_page_id(key);

            page_cache_manager_.unpin_page(current_page_id, false);
            current_page_id = next_page_id;
            continue;
        }

        page_cache_manager_.unpin_page(current_page_id, false);
        return std::nullopt;
    }
}

std::optional<RowId> BPlusTree::insert(std::uint32_t key, const RowId& row_id) {
    // First insert in an empty tree creates a single leaf root.
    if (root_page_id_ == INVALID_PAGE_ID) {
        Page* root_page = page_cache_manager_.new_page();
        if (root_page == nullptr) {
            return std::nullopt;
        }

        root_page_id_ = root_page->page_id;

        BPlusTreeLeafPage leaf_page(*root_page);
        leaf_page.initialize(4);

        if (!leaf_page.insert_entry(key, row_id)) {
            page_cache_manager_.unpin_page(root_page_id_, true);
            return std::nullopt;
        }

        page_cache_manager_.unpin_page(root_page_id_, true);
        return row_id;
    }

    Page* root_page = page_cache_manager_.fetch_page(root_page_id_);
    if (root_page == nullptr) {
        return std::nullopt;
    }

    const BPlusTreePageHeader* header = 
        reinterpret_cast<const BPlusTreePageHeader*>(root_page->data.data());
    
    // For now only support insertion into a root leaf or splitting a full root leaf.
    if (header->page_type != BPlusTreePageType::Leaf) {
        page_cache_manager_.unpin_page(root_page_id_, false);
        return std::nullopt;
    }

    page_cache_manager_.unpin_page(root_page_id_, false);

    Page* root_leaf_page = page_cache_manager_.fetch_page(root_page_id_);
    if (root_leaf_page == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage root_leaf(*root_leaf_page);
    if (!root_leaf.is_full()) {
        page_cache_manager_.unpin_page(root_page_id_, false);
        return insert_into_root_leaf(key, row_id);
    }

    page_cache_manager_.unpin_page(root_page_id_, false);
    return split_root_leaf_and_insert(key, row_id);
}

std::optional<RowId> BPlusTree::insert_into_root_leaf(std::uint32_t key, const RowId& row_id) {
    Page* root_page = page_cache_manager_.fetch_page(root_page_id_);
    if (root_page == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage leaf_page(*root_page);

    if (!leaf_page.insert_entry(key, row_id)) {
        page_cache_manager_.unpin_page(root_page_id_, false);
        return std::nullopt;
    }

    page_cache_manager_.unpin_page(root_page_id_, true);
    return row_id;
}

std::optional<RowId> BPlusTree::split_root_leaf_and_insert(std::uint32_t key, const RowId& row_id) {
    const std::uint32_t old_root_page_id = root_page_id_;

    Page* old_root_page = page_cache_manager_.fetch_page(old_root_page_id);
    if (old_root_page == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage old_root_leaf(*old_root_page);

    std::vector<TempLeafEntry> all_entries;
    // Reserve enough space for all existing entries plus the new entry that
    // triggered the split so vector growth does not interfere with the rebuild.
    all_entries.reserve(old_root_leaf.key_count() + 1);

    for (std::uint16_t i = 0; i < old_root_leaf.key_count(); ++i) {
        const BPlusTreeLeafEntry* entry = old_root_leaf.entry_at(i);
        if (entry == nullptr) {
            page_cache_manager_.unpin_page(old_root_page_id, false);
            return std::nullopt;
        }

        all_entries.push_back({entry->key, entry->row_id});
    }

    // Reject duplicate keys before rebuilding the split leaves.
    for (const TempLeafEntry& entry : all_entries) {
        if (entry.key == key) {
            page_cache_manager_.unpin_page(old_root_page_id, false);
            return std::nullopt;
        }
    }

    all_entries.push_back({key, row_id});
    std::sort(
        all_entries.begin(),
        all_entries.end(),
        [](const TempLeafEntry& a, const TempLeafEntry& b) {
            return a.key < b.key;
        }
    );

    Page* right_leaf_page = page_cache_manager_.new_page();
    if (right_leaf_page == nullptr) {
        page_cache_manager_.unpin_page(old_root_page_id, false);
        return std::nullopt;
    }

    const std::uint32_t right_leaf_page_id = right_leaf_page->page_id;

    // Reinitialize the old root page as the left leaf.
    old_root_leaf.initialize(4);

    BPlusTreeLeafPage right_leaf(*right_leaf_page);
    right_leaf.initialize(4);

    const std::size_t split_index = all_entries.size() / 2;

    for (std::size_t i = 0; i < split_index; ++i) {
        if (!old_root_leaf.insert_entry(all_entries[i].key, all_entries[i].row_id)) {
            page_cache_manager_.unpin_page(old_root_page_id, true);
            page_cache_manager_.unpin_page(right_leaf_page_id, true);
            return std::nullopt;
        }
    }

    for (std::size_t i = split_index; i < all_entries.size(); ++i) {
        if (!right_leaf.insert_entry(all_entries[i].key, all_entries[i].row_id)) {
            page_cache_manager_.unpin_page(old_root_page_id, true);
            page_cache_manager_.unpin_page(right_leaf_page_id, true);
            return std::nullopt;
        }
    }

    old_root_leaf.set_next_leaf_page_id(right_leaf_page_id);

    const std::uint32_t promoted_key = all_entries[split_index].key;

    Page* new_root_page = page_cache_manager_.new_page();
    if (new_root_page == nullptr) {
        page_cache_manager_.unpin_page(old_root_page_id, true);
        page_cache_manager_.unpin_page(right_leaf_page_id, true);
        return std::nullopt;
    }

    const std::uint32_t new_root_page_id = new_root_page->page_id;

    BPlusTreeInternalPage new_root(*new_root_page);
    new_root.initialize(4);
    new_root.set_leftmost_child_page_id(old_root_page_id);

    if (!new_root.insert_entry(promoted_key, right_leaf_page_id)) {
        page_cache_manager_.unpin_page(old_root_page_id, true);
        page_cache_manager_.unpin_page(right_leaf_page_id, true);
        page_cache_manager_.unpin_page(new_root_page_id, true);
        return std::nullopt;
    }

    // Update parent links in the split leaves
    old_root_leaf.fetch_header()->parent_page_id = new_root_page_id;
    right_leaf.fetch_header()->parent_page_id = new_root_page_id;

    page_cache_manager_.unpin_page(old_root_page_id, true);
    page_cache_manager_.unpin_page(right_leaf_page_id, true);
    page_cache_manager_.unpin_page(new_root_page_id, true);

    root_page_id_ = new_root_page_id;
    return row_id;
}


}  // namespace db::index
