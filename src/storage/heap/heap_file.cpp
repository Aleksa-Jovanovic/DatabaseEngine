#include "storage/heap/heap_file.h"

#include <cstring>

namespace db {

namespace {
// Layout helpers for the fixed-size page format used by HeapFile.
constexpr std::size_t HEADER_SIZE = sizeof(PageHeader);
constexpr std::size_t RECORD_SIZE = sizeof(Record);

std::size_t max_records_per_page() {
    return (PAGE_SIZE - HEADER_SIZE) / RECORD_SIZE;
}

// constexpr std::size_t MAX_RECORDS_PER_PAGE =
//     (PAGE_SIZE - sizeof(PageHeader)) / sizeof(Record);

}

HeapFile::HeapFile(const std::string& file_name) : disk_manager_(file_name) {}

bool HeapFile::try_insert_into_page(Page& page, const Record& record) {
    // The page begins with a PageHeader, followed by a contiguous Record array.
    auto* header = reinterpret_cast<PageHeader*>(page.data.data());

    if (header->num_records >= max_records_per_page()) {
        return false;
    }

    auto* records = reinterpret_cast<Record*>(page.data.data() + HEADER_SIZE);
    records[header->num_records] = record;
    header->num_records += 1;

    return true;
}

void HeapFile::insert(const Record& record) {
    // If no pages exist yet, allocate the first heap page and insert into it.
    if (disk_manager_.get_page_count() == 0) {
        Page new_page{};
        new_page.page_id = disk_manager_.allocate_page();

        auto* header = reinterpret_cast<PageHeader*>(new_page.data.data());
        header->num_records = 0;

        try_insert_into_page(new_page, record);
        disk_manager_.write_page(new_page.page_id, new_page.data.data());
        return;
    }

    const std::uint32_t last_page_id = disk_manager_.get_page_count() - 1;
    Page page{};
    page.page_id = last_page_id;

    // First try to append to the last page; if it is full, allocate a new page.
    disk_manager_.read_page(last_page_id, page.data.data());
    if (try_insert_into_page(page, record)) {
        disk_manager_.write_page(last_page_id, page.data.data());
        return;
    }

    Page new_page{};
    new_page.page_id = disk_manager_.allocate_page();

    auto* header = reinterpret_cast<PageHeader*>(new_page.data.data());
    header->num_records = 0;

    try_insert_into_page(new_page, record);
    disk_manager_.write_page(new_page.page_id, new_page.data.data());
}

std::optional<Record> HeapFile::find(std::uint32_t key) {
    // Heap file lookup is a full scan in the current version.
    for (std::uint32_t page_id = 0; page_id < disk_manager_.get_page_count(); ++page_id) {
        Page page{};
        page.page_id = page_id;
        disk_manager_.read_page(page_id, page.data.data());

        auto* header = reinterpret_cast<PageHeader*>(page.data.data());
        auto* records = reinterpret_cast<Record*>(page.data.data() + HEADER_SIZE);

        for (std::uint32_t i = 0; i < header->num_records; ++i) {
            if (records[i].key == key) {
                return records[i];
            }
        }
    }

    return std::nullopt;
}

}
