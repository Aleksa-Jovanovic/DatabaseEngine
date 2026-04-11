#include "storage/heap/heap_file.h"

namespace db {

HeapFile::HeapFile(const std::string& file_name) : disk_manager_(file_name) {}

void HeapFile::initialize_new_page(Page& page) {
    SlottedPage slotted_page(page);
    slotted_page.initialize();
}

std::optional<std::uint16_t> HeapFile::try_insert_into_page(Page& page, const Record& record) {
    SlottedPage slotted_page(page);

    const std::uint16_t slot_index = slotted_page.slot_count();
    if (!slotted_page.insert_record(record)) {
        return std::nullopt;
    }

    return slot_index;
}

std::optional<std::uint16_t> HeapFile::try_insert_var_record_into_page(Page& page, const VarRecord& record) {
    SlottedPage slotted_page(page);

    const std::uint16_t slot_index = slotted_page.slot_count();
    if (!slotted_page.insert_var_record(record)) {
        return std::nullopt;
    }

    return slot_index;
}

std::optional<RowId> HeapFile::insert(const Record& record) {
    // If no pages exist yet, allocate the first heap page and insert into it.
    if (disk_manager_.get_page_count() == 0) {
        Page new_page{};
        new_page.page_id = disk_manager_.allocate_page();

        initialize_new_page(new_page);

        auto slot_index = try_insert_into_page(new_page, record);
        if (!slot_index.has_value()) {
            return std::nullopt;
        }

        disk_manager_.write_page(new_page.page_id, new_page.data.data());
        return RowId{new_page.page_id, slot_index.value()};
    }

    const std::uint32_t last_page_id = disk_manager_.get_page_count() - 1;
    Page page{};
    page.page_id = last_page_id;

    // First try to append to the last page; if it is full, allocate a new page.
    disk_manager_.read_page(last_page_id, page.data.data());
    
    auto slot_index = try_insert_into_page(page, record);
    if (slot_index.has_value()) {
        disk_manager_.write_page(last_page_id, page.data.data());
        return RowId{last_page_id, slot_index.value()};
    }

    Page new_page{};
    new_page.page_id = disk_manager_.allocate_page();

    initialize_new_page(new_page);

    slot_index = try_insert_into_page(new_page, record);
    if (!slot_index.has_value()) {
        return std::nullopt;
    }

    disk_manager_.write_page(new_page.page_id, new_page.data.data());
    return RowId{new_page.page_id, slot_index.value()};
}

std::optional<RowId> HeapFile::insert_var_record(const VarRecord& record) {
    // If no pages exist yet, allocate the first heap page and insert into it.
    if (disk_manager_.get_page_count() == 0) {
        Page new_page{};
        new_page.page_id = disk_manager_.allocate_page();

        initialize_new_page(new_page);

        auto slot_index = try_insert_var_record_into_page(new_page, record);
        if (!slot_index.has_value()) {
            return std::nullopt;
        }

        disk_manager_.write_page(new_page.page_id, new_page.data.data());
        return RowId{new_page.page_id, slot_index.value()};
    }

    const std::uint32_t last_page_id = disk_manager_.get_page_count() - 1;
    Page page{};
    page.page_id = last_page_id;

    disk_manager_.read_page(last_page_id, page.data.data());

    auto slot_index = try_insert_var_record_into_page(page, record);
    if (slot_index.has_value()) {
        disk_manager_.write_page(last_page_id, page.data.data());
        return RowId{last_page_id, slot_index.value()};
    }

    Page new_page{};
    new_page.page_id = disk_manager_.allocate_page();

    initialize_new_page(new_page);

    slot_index = try_insert_var_record_into_page(new_page, record);
    if (!slot_index.has_value()) {
        return std::nullopt;
    }

    disk_manager_.write_page(new_page.page_id, new_page.data.data());
    return RowId{new_page.page_id, slot_index.value()}; 
}

std::optional<Record> HeapFile::get(const RowId& row_id) {
    if (row_id.page_id >= disk_manager_.get_page_count()) {
        return std::nullopt;
    }

    Page page{};
    page.page_id = row_id.page_id;
    disk_manager_.read_page(row_id.page_id, page.data.data());

    SlottedPage slotted_page(page);
    return slotted_page.record_at(row_id.slot_index);
}

std::optional<VarRecord> HeapFile::get_var_record(const RowId& row_id) {
    if (row_id.page_id >= disk_manager_.get_page_count()) {
        return std::nullopt;
    }

    Page page{};
    page.page_id = row_id.page_id;
    disk_manager_.read_page(row_id.page_id, page.data.data());

    SlottedPage slotted_page(page);
    return slotted_page.var_record_at(row_id.slot_index);
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

bool HeapFile::delete_record(const RowId& row_id) {
    if (row_id.page_id >= disk_manager_.get_page_count()) {
        return false;
    }

    Page page{};
    page.page_id = row_id.page_id;
    disk_manager_.read_page(row_id.page_id, page.data.data());

    SlottedPage slotted_page(page);
    if (!slotted_page.delete_record(row_id.slot_index)) {
        return false;
    }

    disk_manager_.write_page(row_id.page_id, page.data.data());
    return true;
}

bool HeapFile::update_record(const RowId& row_id, const Record& record) {
    if (row_id.page_id >= disk_manager_.get_page_count()) {
        return false;
    }

    Page page{};
    page.page_id = row_id.page_id;
    disk_manager_.read_page(row_id.page_id, page.data.data());

    SlottedPage slotted_page(page);
    if (!slotted_page.update_record(row_id.slot_index, record)) {
        return false;
    }

    disk_manager_.write_page(row_id.page_id, page.data.data());
    return true;
}

std::optional<RowId> HeapFile::update_var_record(const RowId& row_id, const VarRecord& record) {
    if (row_id.page_id >= disk_manager_.get_page_count()) {
        return std::nullopt;
    }

    Page page{};
    page.page_id = row_id.page_id;
    disk_manager_.read_page(row_id.page_id, page.data.data());

    SlottedPage slotted_page(page);

    const auto* original_slot_entry = slotted_page.slot_at(row_id.slot_index);
    if (original_slot_entry == nullptr) {
        return std::nullopt;
    }

    const std::uint16_t original_length = original_slot_entry->length;

    if (!slotted_page.update_var_record(row_id.slot_index, record)) {
        return std::nullopt;
    }

    disk_manager_.write_page(row_id.page_id, page.data.data());

    const auto* updated_slot_entry = slotted_page.slot_at(row_id.slot_index);
    if (updated_slot_entry != nullptr && updated_slot_entry->length == original_length && updated_slot_entry->length != 0) {
        return row_id;
    }

    const std::uint16_t new_slot_index = static_cast<std::uint16_t>(slotted_page.slot_count() - 1);
    return RowId{row_id.page_id, new_slot_index};
}

}  // namespace db
