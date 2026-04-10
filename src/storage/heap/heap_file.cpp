#include "storage/heap/heap_file.h"

namespace db {

HeapFile::HeapFile(const std::string& file_name) : disk_manager_(file_name) {}

bool HeapFile::try_insert_into_page(Page& page, const Record& record) {
    SlottedPage slotted_page(page);
    return slotted_page.insert_record(record);
}

void HeapFile::insert(const Record& record) {
    // If no pages exist yet, allocate the first heap page and insert into it.
    if (disk_manager_.get_page_count() == 0) {
        Page new_page{};
        new_page.page_id = disk_manager_.allocate_page();

        SlottedPage slotted_page(new_page);
        slotted_page.initialize();

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

    SlottedPage slotted_page(new_page);
    slotted_page.initialize();

    try_insert_into_page(new_page, record);
    disk_manager_.write_page(new_page.page_id, new_page.data.data());
}

std::optional<Record> HeapFile::find(std::uint32_t key) {
    // Heap file lookup is a full scan in the current version.
    for (std::uint32_t page_id = 0; page_id < disk_manager_.get_page_count(); ++page_id) {
        Page page{};
        page.page_id = page_id;
        disk_manager_.read_page(page_id, page.data.data());

        SlottedPage slotted_page(page);

        for (std::uint16_t i = 0; i < slotted_page.slot_count(); ++i) {
            auto record = slotted_page.record_at(i);
            if (record.has_value() && record->key == key) {
                return record;
            }
        }
    }

    return std::nullopt;
}

}  // namespace db
