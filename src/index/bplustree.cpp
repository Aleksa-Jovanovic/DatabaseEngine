#include "index/bplustree.h"

#include <algorithm>
#include <vector>

#include "index/bplustree_internal.h"
#include "index/bplustree_leaf.h"
#include "storage/page/page.h"

namespace db::index {

namespace {
// A fixed signature stored in page 0 so reopen logic can verify that the page
// really contains B+ tree metadata instead of zero-filled or unrelated bytes.
constexpr std::uint32_t BPLUS_TREE_METADATA_PAGE_ID = 0;
constexpr std::uint32_t BPLUS_TREE_METADATA_MAGIC = 0x42505452;  // "BPTR"

struct BPlusTreeMetadataPage {
    // Identifies this page as initialized B+ tree metadata.
    std::uint32_t magic;
    std::uint32_t root_page_id;
};

struct TempLeafEntry {
    std::uint32_t key;
    RowId row_id;
};

struct TempInternalNode {
    std::vector<std::uint32_t> keys;
    std::vector<std::uint32_t> child_page_ids;
};
}

BPlusTree::BPlusTree(const std::string& file_name, std::size_t cache_size)
    : page_cache_manager_(file_name, cache_size) {
    if (!load_or_initialize_metadata_page()) {
        root_page_id_ = INVALID_PAGE_ID;
    }
}

bool BPlusTree::load_or_initialize_metadata_page() {
    if (page_cache_manager_.get_page_count() == 0) {
        Page* metadata_page_ptr = page_cache_manager_.new_page();
        if (metadata_page_ptr == nullptr) {
            root_page_id_ = INVALID_PAGE_ID;
            return false;
        }

        auto* metadata =
            reinterpret_cast<BPlusTreeMetadataPage*>(metadata_page_ptr->data.data());
        metadata->magic = BPLUS_TREE_METADATA_MAGIC;
        metadata->root_page_id = INVALID_PAGE_ID;
        root_page_id_ = INVALID_PAGE_ID;

        page_cache_manager_.unpin_page(BPLUS_TREE_METADATA_PAGE_ID, true);
        return true;
    }

    Page* metadata_page_ptr = page_cache_manager_.fetch_page(BPLUS_TREE_METADATA_PAGE_ID);
    if (metadata_page_ptr == nullptr) {
        root_page_id_ = INVALID_PAGE_ID;
        return false;
    }

    const auto* metadata =
        reinterpret_cast<const BPlusTreeMetadataPage*>(metadata_page_ptr->data.data());

    // Refuse to trust the stored root page id unless page 0 has our expected
    // metadata signature.
    if (metadata->magic != BPLUS_TREE_METADATA_MAGIC) {
        page_cache_manager_.unpin_page(BPLUS_TREE_METADATA_PAGE_ID, false);
        root_page_id_ = INVALID_PAGE_ID;
        return false;
    }

    root_page_id_ = metadata->root_page_id;
    page_cache_manager_.unpin_page(BPLUS_TREE_METADATA_PAGE_ID, false);
    return true;
}

bool BPlusTree::persist_root_page_id() {
    Page* metadata_page_ptr = page_cache_manager_.fetch_page(BPLUS_TREE_METADATA_PAGE_ID);
    if (metadata_page_ptr == nullptr) {
        return false;
    }

    auto* metadata =
        reinterpret_cast<BPlusTreeMetadataPage*>(metadata_page_ptr->data.data());
    // Only overwrite page 0 when it is already recognized as our metadata page.
    if (metadata->magic != BPLUS_TREE_METADATA_MAGIC) {
        page_cache_manager_.unpin_page(BPLUS_TREE_METADATA_PAGE_ID, false);
        return false;
    }

    metadata->magic = BPLUS_TREE_METADATA_MAGIC;
    metadata->root_page_id = root_page_id_;

    page_cache_manager_.unpin_page(BPLUS_TREE_METADATA_PAGE_ID, true);
    return true;
}

std::uint16_t BPlusTree::leaf_node_max_size() const {
    constexpr std::size_t header_bytes =
        sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t);
    return static_cast<std::uint16_t>(
        (PAGE_SIZE - header_bytes) / sizeof(BPlusTreeLeafEntry)
    );
}

std::uint16_t BPlusTree::internal_node_max_size() const {
    constexpr std::size_t header_bytes =
        sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t);
    return static_cast<std::uint16_t>(
        (PAGE_SIZE - header_bytes) / sizeof(BPlusTreeInternalEntry)
    );
}

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
    if (root_page_id_ == INVALID_PAGE_ID) {
        return create_initial_tree(key, row_id);
    }

    const auto leaf_page_id = find_leaf_page_id(key);
    if (!leaf_page_id.has_value()) {
        return std::nullopt;
    }

    Page* leaf_page_ptr = page_cache_manager_.fetch_page(leaf_page_id.value());
    if (leaf_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage leaf_page(*leaf_page_ptr);
    const bool leaf_is_full = leaf_page.is_full();

    page_cache_manager_.unpin_page(leaf_page_id.value(), false);

    if (!leaf_is_full) {
        return insert_into_leaf(leaf_page_id.value(), key, row_id);
    }

    return split_leaf_and_insert(leaf_page_id.value(), key, row_id);
}

std::optional<RowId> BPlusTree::update(std::uint32_t key, const RowId& row_id) {
    const auto leaf_page_id = find_leaf_page_id(key);
    if (!leaf_page_id.has_value()) {
        return std::nullopt;
    }

    Page* leaf_page_ptr = page_cache_manager_.fetch_page(leaf_page_id.value());
    if (leaf_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage leaf_page(*leaf_page_ptr);

    for (std::uint16_t i = 0; i < leaf_page.key_count(); ++i) {
        BPlusTreeLeafEntry* entry = leaf_page.entry_at(i);
        if (entry == nullptr) {
            page_cache_manager_.unpin_page(leaf_page_id.value(), false);
            return std::nullopt;
        }

        if (entry->key == key) {
            const RowId previous_row_id = entry->row_id;
            entry->row_id = row_id;

            page_cache_manager_.unpin_page(leaf_page_id.value(), true);
            return previous_row_id;
        }

        if (entry->key > key) {
            break;
        }
    }

    page_cache_manager_.unpin_page(leaf_page_id.value(), false);
    return std::nullopt;
}

std::optional<RowId> BPlusTree::delete_key(std::uint32_t key) {
    const auto leaf_page_id = find_leaf_page_id(key);
    if (!leaf_page_id.has_value()) {
        return std::nullopt;
    }

    Page* leaf_page_ptr = page_cache_manager_.fetch_page(leaf_page_id.value());
    if (leaf_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage leaf_page(*leaf_page_ptr);

    const BPlusTreeLeafEntry* existing_entry = leaf_page.find_entry(key);
    if (existing_entry == nullptr) {
        page_cache_manager_.unpin_page(leaf_page_id.value(), false);
        return std::nullopt;
    }

    const RowId deleted_row_id = existing_entry->row_id;

    if (!leaf_page.delete_entry(key)) {
        page_cache_manager_.unpin_page(leaf_page_id.value(), false);
        return std::nullopt;
    }

    page_cache_manager_.unpin_page(leaf_page_id.value(), true);
    return deleted_row_id;
}

std::optional<RowId> BPlusTree::create_initial_tree(std::uint32_t key, const RowId& row_id) {
    Page* root_page = page_cache_manager_.new_page();
    if (root_page == nullptr) {
        return std::nullopt;
    }

    root_page_id_ = root_page->page_id;

    BPlusTreeLeafPage leaf_page(*root_page);
    leaf_page.initialize(leaf_node_max_size());

    if (!leaf_page.insert_entry(key, row_id)) {
        page_cache_manager_.unpin_page(root_page_id_, true);
        return std::nullopt;
    }

    page_cache_manager_.unpin_page(root_page_id_, true);
    if (!persist_root_page_id()) {
        return std::nullopt;
    }

    return row_id;
}

std::optional<std::uint32_t> BPlusTree::find_leaf_page_id(std::uint32_t key) {
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
            page_cache_manager_.unpin_page(current_page_id, false);
            return current_page_id;
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

std::optional<RowId> BPlusTree::insert_into_leaf(std::uint32_t leaf_page_id, std::uint32_t key, const RowId& row_id) {
    Page* leaf_page_ptr = page_cache_manager_.fetch_page(leaf_page_id);
    if (leaf_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage leaf_page(*leaf_page_ptr);

    if (!leaf_page.insert_entry(key, row_id)) {
        page_cache_manager_.unpin_page(leaf_page_id, false);
        return std::nullopt;
    }

    page_cache_manager_.unpin_page(leaf_page_id, true);
    return row_id;
}

std::optional<RowId> BPlusTree::create_new_root(
    std::uint32_t left_child_page_id,
    std::uint32_t key,
    std::uint32_t right_child_page_id,
    const RowId& row_id
) {
    // A split reached the top of the tree, so create a fresh internal root
    // above the two resulting child pages.
    Page* new_root_page_ptr = page_cache_manager_.new_page();
    if (new_root_page_ptr == nullptr) {
        return std::nullopt;
    }

    const std::uint32_t new_root_page_id = new_root_page_ptr->page_id;

    BPlusTreeInternalPage new_root(*new_root_page_ptr);
    new_root.initialize(internal_node_max_size());
    new_root.set_leftmost_child_page_id(left_child_page_id);

    if (!new_root.insert_after_child(left_child_page_id, key, right_child_page_id)) {
        page_cache_manager_.unpin_page(new_root_page_id, true);
        return std::nullopt;
    }

    Page* left_child_page_ptr = page_cache_manager_.fetch_page(left_child_page_id);
    if (left_child_page_ptr == nullptr) {
        page_cache_manager_.unpin_page(new_root_page_id, true);
        return std::nullopt;
    }

    BPlusTreePageHeader* left_header = 
        reinterpret_cast<BPlusTreePageHeader*>(left_child_page_ptr->data.data());
    left_header->parent_page_id = new_root_page_id;

    page_cache_manager_.unpin_page(left_child_page_id, true);

    Page* right_child_page_ptr = page_cache_manager_.fetch_page(right_child_page_id);
    if (right_child_page_ptr == nullptr) {
        page_cache_manager_.unpin_page(new_root_page_id, true);
        return std::nullopt;
    }

    BPlusTreePageHeader* right_header =
        reinterpret_cast<BPlusTreePageHeader*>(right_child_page_ptr->data.data());
    right_header->parent_page_id = new_root_page_id;

    page_cache_manager_.unpin_page(right_child_page_id, true);
    page_cache_manager_.unpin_page(new_root_page_id, true);

    root_page_id_ = new_root_page_id;
    if (!persist_root_page_id()) {
        return std::nullopt;
    }

    return row_id;
}

std::optional<RowId> BPlusTree::insert_into_parent(
    std::uint32_t parent_page_id,
    std::uint32_t left_child_page_id,
    std::uint32_t key,
    std::uint32_t right_child_page_id,
    const RowId& row_id
) {
    if (parent_page_id == INVALID_PAGE_ID) {
        return create_new_root(left_child_page_id, key, right_child_page_id, row_id);
    }

    Page* parent_page_ptr = page_cache_manager_.fetch_page(parent_page_id);
    if (parent_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeInternalPage parent_page(*parent_page_ptr);
    const bool parent_is_full = parent_page.is_full();

    page_cache_manager_.unpin_page(parent_page_id, false);

    if (!parent_is_full) {
        // Re-fetch the parent for the write path after the read-only fullness check.
        Page* writable_parent_page_ptr = page_cache_manager_.fetch_page(parent_page_id);
        if (writable_parent_page_ptr == nullptr) {
            return std::nullopt;
        }

        BPlusTreeInternalPage writable_parent(*writable_parent_page_ptr);
        if (!writable_parent.insert_after_child(left_child_page_id, key, right_child_page_id)) {
            page_cache_manager_.unpin_page(parent_page_id, false);
            return std::nullopt;
        }

        page_cache_manager_.unpin_page(parent_page_id, true);
        return row_id;
    }

    // If the parent is full, split it and continue pushing the promoted
    // separator upward until some ancestor has room or a new root is created.
    const auto split_result = split_internal_and_insert(
        parent_page_id,
        left_child_page_id,
        key,
        right_child_page_id
    );
    if (!split_result.has_value()) {
        return std::nullopt;
    }

    Page* split_left_page_ptr = page_cache_manager_.fetch_page(split_result->left_page_id);
    if (split_left_page_ptr == nullptr) {
        return std::nullopt;
    }

    const BPlusTreePageHeader* left_header =
        reinterpret_cast<const BPlusTreePageHeader*>(split_left_page_ptr->data.data());
    const std::uint32_t grandparent_page_id = left_header->parent_page_id;

    page_cache_manager_.unpin_page(split_result->left_page_id, false);

    // The left split result remains in the original page id, so recurse upward
    // using that page as the left child of the newly promoted separator.
    return insert_into_parent(
        grandparent_page_id,
        split_result->left_page_id,
        split_result->promoted_key,
        split_result->right_page_id,
        row_id
    );
}

std::optional<RowId> BPlusTree::split_leaf_and_insert(std::uint32_t leaf_page_id, std::uint32_t key, const RowId& row_id) {
    Page* left_leaf_page_ptr = page_cache_manager_.fetch_page(leaf_page_id);
    if (left_leaf_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeLeafPage left_leaf(*left_leaf_page_ptr);
    const std::uint32_t parent_page_id = left_leaf.fetch_header()->parent_page_id;
    const std::uint32_t old_next_leaf_page_id = left_leaf.next_leaf_page_id();

    std::vector<TempLeafEntry> all_entries;
    all_entries.reserve(left_leaf.key_count() + 1);

    for (std::uint16_t i = 0; i < left_leaf.key_count(); ++i) {
        const BPlusTreeLeafEntry* entry = left_leaf.entry_at(i);
        if (entry == nullptr) {
            page_cache_manager_.unpin_page(leaf_page_id, false);
            return std::nullopt;
        }

        all_entries.push_back({entry->key, entry->row_id});
    }

    // Reject duplicates before rebuilding the split leaves.
    for (const TempLeafEntry& entry : all_entries) {
        if (entry.key == key) {
            page_cache_manager_.unpin_page(leaf_page_id, false);
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

    Page* right_leaf_page_ptr = page_cache_manager_.new_page();
    if (right_leaf_page_ptr == nullptr) {
        page_cache_manager_.unpin_page(leaf_page_id, false);
        return std::nullopt;
    }

    const std::uint32_t right_leaf_page_id = right_leaf_page_ptr->page_id;

    // Reuse the original page as the left split result and build a new right leaf.
    left_leaf.initialize(leaf_node_max_size());

    BPlusTreeLeafPage right_leaf(*right_leaf_page_ptr);
    right_leaf.initialize(leaf_node_max_size());

    // Keep both split leaves attached to the same parent unless we create a new root below.
    left_leaf.fetch_header()->parent_page_id = parent_page_id;
    right_leaf.fetch_header()->parent_page_id = parent_page_id;

    const std::size_t split_index = all_entries.size() / 2;

    for (std::size_t i = 0; i < split_index; ++i) {
        if (!left_leaf.insert_entry(all_entries[i].key, all_entries[i].row_id)) {
            page_cache_manager_.unpin_page(leaf_page_id, true);
            page_cache_manager_.unpin_page(right_leaf_page_id, true);
            return std::nullopt;
        }
    }

    for (std::size_t i = split_index; i < all_entries.size(); ++i) {
        if (!right_leaf.insert_entry(all_entries[i].key, all_entries[i].row_id)) {
            page_cache_manager_.unpin_page(leaf_page_id, true);
            page_cache_manager_.unpin_page(right_leaf_page_id, true);
            return std::nullopt;
        }
    }

    // Preserve the leaf-level linked list for later range scans.
    left_leaf.set_next_leaf_page_id(right_leaf_page_id);
    right_leaf.set_next_leaf_page_id(old_next_leaf_page_id);

    // In a leaf split, the first key in the right leaf becomes the separator key.
    const std::uint32_t promoted_key = all_entries[split_index].key;

    page_cache_manager_.unpin_page(leaf_page_id, true);
    page_cache_manager_.unpin_page(right_leaf_page_id, true);

    return insert_into_parent(
        parent_page_id,
        leaf_page_id,
        promoted_key,
        right_leaf_page_id,
        row_id
    );
}

std::optional<BPlusTree::InternalInsertResult> BPlusTree::split_internal_and_insert(
    std::uint32_t internal_page_id,
    std::uint32_t left_child_page_id,
    std::uint32_t key,
    std::uint32_t right_child_page_id
) {
    Page* left_internal_page_ptr = page_cache_manager_.fetch_page(internal_page_id);
    if (left_internal_page_ptr == nullptr) {
        return std::nullopt;
    }

    BPlusTreeInternalPage left_internal(*left_internal_page_ptr);
    const std::uint32_t parent_page_id = left_internal.fetch_header()->parent_page_id;

    // Build one logical internal node in temporary vectors before splitting it.
    TempInternalNode temp_node;
    temp_node.keys.reserve(left_internal.key_count() + 1);
    // An internal node always has one more child pointer than separator keys.
    // After inserting one new separator, we need room for two extra children
    // compared with the original key count.
    temp_node.child_page_ids.reserve(left_internal.key_count() + 2);

    // The first child lives outside the entry array in the leftmost-child field.
    temp_node.child_page_ids.push_back(left_internal.leftmost_child_page_id());

    for (std::uint16_t i = 0; i < left_internal.key_count(); ++i) {
        const BPlusTreeInternalEntry* entry = left_internal.entry_at(i);
        if (entry == nullptr) {
            page_cache_manager_.unpin_page(internal_page_id, false);
            return std::nullopt;
        }

        temp_node.keys.push_back(entry->key);
        temp_node.child_page_ids.push_back(entry->right_child_page_id);
    }

    if (temp_node.keys.size() + 1 != temp_node.child_page_ids.size()) {
        page_cache_manager_.unpin_page(internal_page_id, false);
        return std::nullopt;
    }

    // Find the child that split so the promoted separator is inserted in the
    // correct structural position, not just by key order.
    auto child_it = std::find(
        temp_node.child_page_ids.begin(),
        temp_node.child_page_ids.end(),
        left_child_page_id
    );
    if (child_it == temp_node.child_page_ids.end()) {
        page_cache_manager_.unpin_page(internal_page_id, false);
        return std::nullopt;
    }

    const std::size_t child_index = static_cast<std::size_t>(
        std::distance(temp_node.child_page_ids.begin(), child_it)
    );

    // Insert the promoted separator immediately after the child that split.
    temp_node.keys.insert(temp_node.keys.begin() + child_index, key);
    temp_node.child_page_ids.insert(
        temp_node.child_page_ids.begin() + child_index + 1,
        right_child_page_id
    );

    Page* right_internal_page_ptr = page_cache_manager_.new_page();
    if (right_internal_page_ptr == nullptr) {
        page_cache_manager_.unpin_page(internal_page_id, false);
        return std::nullopt;
    }

    const std::uint32_t right_internal_page_id = right_internal_page_ptr->page_id;

    // Rebuild the original page as the left split result and create a new
    // right internal page for the upper half.
    left_internal.initialize(internal_node_max_size());

    BPlusTreeInternalPage right_internal(*right_internal_page_ptr);
    right_internal.initialize(internal_node_max_size());

    // Both split internal nodes stay under the same parent. The promoted key
    // itself will be inserted into that parent by the caller.
    left_internal.fetch_header()->parent_page_id = parent_page_id;
    right_internal.fetch_header()->parent_page_id = parent_page_id;

    // The middle key is pushed up to the parent and removed from both children.
    const std::size_t promote_index = temp_node.keys.size() / 2;
    const std::uint32_t promoted_key = temp_node.keys[promote_index];

    // The left split keeps every key strictly before the promoted separator.
    left_internal.set_leftmost_child_page_id(temp_node.child_page_ids[0]);

    for (std::size_t i = 0; i < promote_index; ++i) {
        if (!left_internal.insert_after_child(
                temp_node.child_page_ids[i],
                temp_node.keys[i],
                temp_node.child_page_ids[i + 1])) {
            page_cache_manager_.unpin_page(internal_page_id, true);
            page_cache_manager_.unpin_page(right_internal_page_id, true);
            return std::nullopt;
        }
    }

    // The first child to the right of the promoted key becomes the right page's
    // new leftmost child pointer.
    right_internal.set_leftmost_child_page_id(temp_node.child_page_ids[promote_index + 1]);

    // The right split keeps every key strictly after the promoted separator.
    for (std::size_t i = promote_index + 1; i < temp_node.keys.size(); ++i) {
        if (!right_internal.insert_after_child(
                temp_node.child_page_ids[i],
                temp_node.keys[i],
                temp_node.child_page_ids[i + 1])) {
            page_cache_manager_.unpin_page(internal_page_id, true);
            page_cache_manager_.unpin_page(right_internal_page_id, true);
            return std::nullopt;
        }
    }

    // Repoint every child kept in the left split to the original internal page.
    for (std::size_t i = 0; i <= promote_index; ++i) {
        Page* child_page_ptr = page_cache_manager_.fetch_page(temp_node.child_page_ids[i]);
        if (child_page_ptr == nullptr) {
            page_cache_manager_.unpin_page(internal_page_id, true);
            page_cache_manager_.unpin_page(right_internal_page_id, true);
            return std::nullopt;
        }

        BPlusTreePageHeader* child_header =
            reinterpret_cast<BPlusTreePageHeader*>(child_page_ptr->data.data());
        child_header->parent_page_id = internal_page_id;

        page_cache_manager_.unpin_page(temp_node.child_page_ids[i], true);
    }

    // Repoint every child moved to the right split to the new internal page.
    for (std::size_t i = promote_index + 1; i < temp_node.child_page_ids.size(); ++i) {
        Page* child_page_ptr = page_cache_manager_.fetch_page(temp_node.child_page_ids[i]);
        if (child_page_ptr == nullptr) {
            page_cache_manager_.unpin_page(internal_page_id, true);
            page_cache_manager_.unpin_page(right_internal_page_id, true);
            return std::nullopt;
        }

        BPlusTreePageHeader* child_header =
            reinterpret_cast<BPlusTreePageHeader*>(child_page_ptr->data.data());
        child_header->parent_page_id = right_internal_page_id;

        page_cache_manager_.unpin_page(temp_node.child_page_ids[i], true);
    }

    page_cache_manager_.unpin_page(internal_page_id, true);
    page_cache_manager_.unpin_page(right_internal_page_id, true);

    return InternalInsertResult{
        promoted_key,
        internal_page_id,
        right_internal_page_id
    };
}


}  // namespace db::index
