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

#ifdef DB_TESTING
    void set_root_page_id(std::uint32_t page_id) {
        root_page_id_ = page_id;
    }
#endif

private:
    PageCacheManager page_cache_manager_;
    std::uint32_t root_page_id_ = INVALID_PAGE_ID;
};

}  // namespace db::index