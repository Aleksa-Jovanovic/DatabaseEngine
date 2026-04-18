#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>

#include "storage/cache/page_cache_manager.h"

int main() {
    const std::string file_name = "page_cache_manager_test.db";
    std::filesystem::remove(file_name);

    std::uint32_t page_id = 0;

    {
        db::PageCacheManager cache(file_name, 2);

        db::Page* page = cache.new_page();
        assert(page != nullptr);

        page_id = page->page_id;
        page->data[0] = 'd';
        page->data[1] = 'b';
        page->data[2] = '!';

        assert(cache.unpin_page(page_id, true));
        assert(cache.flush_page(page_id));
    }

    {
        db::PageCacheManager reopened_cache(file_name, 2);

        db::Page* page = reopened_cache.fetch_page(page_id);
        assert(page != nullptr);
        assert(page->data[0] == 'd');
        assert(page->data[1] == 'b');
        assert(page->data[2] == '!');

        assert(reopened_cache.unpin_page(page_id, false));
    }

    std::filesystem::remove(file_name);

    {
        db::PageCacheManager cache(file_name, 1);

        db::Page* first_page = cache.new_page();
        assert(first_page != nullptr);

        const std::uint32_t first_page_id = first_page->page_id;

        // With a single pinned frame, the cache cannot allocate or fetch another page.
        db::Page* second_page = cache.new_page();
        assert(second_page == nullptr);

        assert(cache.fetch_page(first_page_id) != nullptr);
        assert(cache.unpin_page(first_page_id, false));
        assert(cache.unpin_page(first_page_id, false));

        db::Page* replacement_page = cache.new_page();
        assert(replacement_page != nullptr);
        assert(replacement_page->page_id != first_page_id);

        assert(cache.unpin_page(replacement_page->page_id, false));
    }

    std::filesystem::remove(file_name);

    std::cout << "PageCacheManager test passed.\n";
    return 0;
}
