#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"

namespace db {

// One cache frame holds at most one logical page plus cache metadata.
struct PageFrame {
    Page page{};
    bool is_dirty = false;
    std::uint32_t pin_count = 0;
    bool is_valid = false;
    std::uint64_t last_used_tick = 0;
};

// PageCacheManager keeps a fixed number of disk pages in memory.
// It can fetch existing pages, allocate new pages, track dirty state,
// and flush cached pages back to disk.
class PageCacheManager {
public:
    PageCacheManager(const std::string& file_name, std::size_t cache_size);

    // Load an existing logical page into the cache and pin it.
    // Returns nullptr if the page does not exist or no frame is available.
    Page* fetch_page(std::uint32_t page_id);

    // Allocate a brand new logical page, place it in the cache, and pin it.
    // Returns nullptr if no frame is available.
    Page* new_page();

    // Release one pin from a cached page. If is_dirty is true, the page
    // remains dirty until it is flushed.
    bool unpin_page(std::uint32_t page_id, bool is_dirty);

    // Write one cached page back to disk.
    bool flush_page(std::uint32_t page_id);

    // Write all valid cached pages back to disk.
    void flush_all_pages();

    // Return the current number of allocated logical pages.
    inline std::uint32_t get_page_count() const {
        return disk_manager_.get_page_count();
    }

#ifdef DB_TESTING
    // Test helper: report whether a page is currently cached.
    inline bool is_page_cached(std::uint32_t page_id) const {
        return page_table_.find(page_id) != page_table_.end();
    }
#endif

private:
    std::optional<std::size_t> find_frame_for_page(std::uint32_t page_id) const;
    std::optional<std::size_t> find_available_frame();
    void load_page_into_frame(std::size_t frame_index, std::uint32_t page_id);
    void touch_frame(PageFrame& frame);

    DiskManager disk_manager_;
    std::vector<PageFrame> frames_;
    std::unordered_map<std::uint32_t, std::size_t> page_table_;

    std::uint64_t current_tick_ = 0;
};

}   // namespace db
