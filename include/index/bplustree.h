#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "common/row_id.h"
#include "index/bplustree_page.h"
#include "storage/cache/page_cache_manager.h"

namespace db::index {

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
    PageCacheManager page_cache_manager_;
    std::uint32_t root_page_id_ = INVALID_PAGE_ID;

    std::optional<RowId> insert_into_root_leaf(std::uint32_t key, const RowId& row_id);
    std::optional<RowId> split_root_leaf_and_insert(std::uint32_t key, const RowId& row_id);
};

}  // namespace db::index