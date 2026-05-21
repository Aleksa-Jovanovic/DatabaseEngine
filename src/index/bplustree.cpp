#include "index/bplustree.h"

#include "index/bplustree_internal.h"
#include "index/bplustree_leaf.h"
#include "storage/page/page.h"

namespace db::index {

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

}  // namespace db::index