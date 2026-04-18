#include "storage/cache/page_cache_manager.h"

namespace db {

PageCacheManager::PageCacheManager(const std::string& file_name, std::size_t cache_size)
    : disk_manager_(file_name), frames_(cache_size) {}

std::optional<std::size_t> PageCacheManager::find_frame_for_page(std::uint32_t page_id) const {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<std::size_t> PageCacheManager::find_available_frame() {
    // Prefer an unused frame before evicting any cached page.
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].is_valid) {
            return i;
        }
    }

    // Fall back to the first unpinned cached frame.
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].pin_count == 0) {
            return i;
        }
    }

    return std::nullopt;
}

void PageCacheManager::load_page_into_frame(std::size_t frame_index, std::uint32_t page_id) {
    PageFrame& frame = frames_[frame_index];

    if (frame.is_valid) {
        // Flush dirty contents before reusing the frame for another page.
        if (frame.is_dirty) {
            disk_manager_.write_page(frame.page.page_id, frame.page.data.data());
        }

        page_table_.erase(frame.page.page_id);
    }

    frame.page.page_id = page_id;
    disk_manager_.read_page(page_id, frame.page.data.data());
    frame.is_dirty = false;
    frame.pin_count = 0;
    frame.is_valid = true;

    page_table_[page_id] = frame_index;
}

Page* PageCacheManager::fetch_page(std::uint32_t page_id) {
    if (page_id >= disk_manager_.get_page_count()) {
        return nullptr;
    }

    auto existing_frame_index = find_frame_for_page(page_id);
    if (existing_frame_index.has_value()) {
        PageFrame& frame = frames_[existing_frame_index.value()];
        ++frame.pin_count;
        return &frame.page;
    }

    auto available_frame_index = find_available_frame();
    if (!available_frame_index.has_value()) {
        return nullptr;
    }

    load_page_into_frame(available_frame_index.value(), page_id);

    PageFrame& frame = frames_[available_frame_index.value()];
    ++frame.pin_count;
    return &frame.page;
}

Page* PageCacheManager::new_page() {
    auto available_frame_index = find_available_frame();
    if (!available_frame_index.has_value()) {
        return nullptr;
    }

    const std::size_t frame_index = available_frame_index.value();
    PageFrame& frame = frames_[frame_index];

    if (frame.is_valid) {
        if (frame.is_dirty) {
            disk_manager_.write_page(frame.page.page_id, frame.page.data.data());
        }

        page_table_.erase(frame.page.page_id);
    }

    const std::uint32_t new_page_id = disk_manager_.allocate_page();

    frame.page.page_id = new_page_id;
    // A newly allocated page starts as an empty zero-filled buffer in memory.
    frame.page.data.fill(0);
    frame.is_dirty = false;
    frame.pin_count = 1;
    frame.is_valid = true;

    page_table_[new_page_id] = frame_index;
    return &frame.page;
}

bool PageCacheManager::unpin_page(std::uint32_t page_id, bool is_dirty) {
    auto frame_index = find_frame_for_page(page_id);
    if (!frame_index.has_value()) {
        return false;
    }

    PageFrame& frame = frames_[frame_index.value()];
    if (frame.pin_count == 0) {
        return false;
    }

    --frame.pin_count;

    if (is_dirty) {
        frame.is_dirty = true;
    }

    return true;
}

bool PageCacheManager::flush_page(std::uint32_t page_id) {
    auto frame_index = find_frame_for_page(page_id);
    if (!frame_index.has_value()) {
        return false;
    }

    PageFrame& frame = frames_[frame_index.value()];
    if (!frame.is_valid) {
        return false;
    }

    disk_manager_.write_page(frame.page.page_id, frame.page.data.data());
    frame.is_dirty = false;
    return true;
}

void PageCacheManager::flush_all_pages() {
    for (PageFrame& frame : frames_) {
        if (!frame.is_valid) {
            continue;
        }

        disk_manager_.write_page(frame.page.page_id, frame.page.data.data());
        frame.is_dirty = false;
    }
}

}   // namespace db
