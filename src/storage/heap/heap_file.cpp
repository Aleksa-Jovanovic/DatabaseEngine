#include "storage/heap/heap_file.h"

namespace db {

HeapFile::HeapFile(const std::string& file_name) : page_cache_manager_(file_name, 8) {}

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
    if (page_cache_manager_.get_page_count() == 0) {
        Page* new_page_ptr = page_cache_manager_.new_page();
        if (new_page_ptr == nullptr) {
            return std::nullopt;
        }

        initialize_new_page(*new_page_ptr);

        auto slot_index = try_insert_into_page(*new_page_ptr, record);
        if (!slot_index.has_value()) {
            // Dirty flag is true since the page has been initialized hence changed.
            page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
            return std::nullopt;
        }

        const RowId row_id{new_page_ptr->page_id, slot_index.value()};
        page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
        return row_id;
    }

    // First try to append to the last page; if it is full, allocate a new page.
    const std::uint32_t last_page_id = page_cache_manager_.get_page_count() - 1;
    Page* page_ptr = page_cache_manager_.fetch_page(last_page_id);
    if (page_ptr == nullptr) {
        return std::nullopt;
    }
    
    auto slot_index = try_insert_into_page(*page_ptr, record);
    if (slot_index.has_value()) {
        const RowId row_id{page_ptr->page_id, slot_index.value()};
        page_cache_manager_.unpin_page(last_page_id, true);
        return row_id;
    }

    page_cache_manager_.unpin_page(last_page_id, false);

    // Allocate a new page if the last one is full
    Page* new_page_ptr = page_cache_manager_.new_page();
    if (new_page_ptr == nullptr) {
        return std::nullopt;
    }

    initialize_new_page(*new_page_ptr);

    slot_index = try_insert_into_page(*new_page_ptr, record);
    if (!slot_index.has_value()) {
        // Dirty flag is true since the page has been initialized hence changed.
        page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
        return std::nullopt;
    }

    const RowId row_id{new_page_ptr->page_id, slot_index.value()};
    page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
    return row_id;
}

std::optional<RowId> HeapFile::insert_var_record(const VarRecord& record) {
    // If no pages exist yet, allocate the first heap page and insert into it.
    if (page_cache_manager_.get_page_count() == 0) {
        Page* new_page_ptr = page_cache_manager_.new_page();
        if (new_page_ptr == nullptr) {
            return std::nullopt;
        }

        initialize_new_page(*new_page_ptr);

        auto slot_index = try_insert_var_record_into_page(*new_page_ptr, record);
        if (!slot_index.has_value()) {
            // Dirty flag is true since the page has been initialized and changed.
            page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
            return std::nullopt;
        }

        const RowId row_id{new_page_ptr->page_id, slot_index.value()};
        page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
        return row_id;
    }

    // First try to append to the last page; if it is full, allocate a new page.
    const std::uint32_t last_page_id = page_cache_manager_.get_page_count() - 1;
    Page* page_ptr = page_cache_manager_.fetch_page(last_page_id);
    if (page_ptr == nullptr) {
        return std::nullopt;
    }

    auto slot_index = try_insert_var_record_into_page(*page_ptr, record);
    if (slot_index.has_value()) {
        const RowId row_id{page_ptr->page_id, slot_index.value()};
        page_cache_manager_.unpin_page(last_page_id, true);
        return row_id;
    }

    page_cache_manager_.unpin_page(last_page_id, false);

    // Allocate a new page if the last one is full.
    Page* new_page_ptr = page_cache_manager_.new_page();
    if (new_page_ptr == nullptr) {
        return std::nullopt;
    }

    initialize_new_page(*new_page_ptr);

    slot_index = try_insert_var_record_into_page(*new_page_ptr, record);
    if (!slot_index.has_value()) {
        // Dirty flag is true since the page has been initialized and changed.
        page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
        return std::nullopt;
    }

    const RowId row_id{new_page_ptr->page_id, slot_index.value()};
    page_cache_manager_.unpin_page(new_page_ptr->page_id, true);
    return row_id;
}

std::optional<Record> HeapFile::get(const RowId& row_id) {
    Page* page_ptr = page_cache_manager_.fetch_page(row_id.page_id);
    if (page_ptr == nullptr) {
        return std::nullopt;
    }

    SlottedPage slotted_page(*page_ptr);
    auto record = slotted_page.record_at(row_id.slot_index);

    page_cache_manager_.unpin_page(row_id.page_id, false);
    return record;
}

std::optional<VarRecord> HeapFile::get_var_record(const RowId& row_id) {
    Page* page_ptr = page_cache_manager_.fetch_page(row_id.page_id);
    if (page_ptr == nullptr) {
        return std::nullopt;
    }

    SlottedPage slotted_page(*page_ptr);
    auto record = slotted_page.var_record_at(row_id.slot_index);

    page_cache_manager_.unpin_page(row_id.page_id, false);
    return record;
}

std::optional<Record> HeapFile::find(std::uint32_t key) {
    // Heap file lookup is a full scan in the current version.
    for (std::uint32_t page_id = 0; page_id < page_cache_manager_.get_page_count(); ++page_id) {
        Page* page_ptr = page_cache_manager_.fetch_page(page_id);
        if (page_ptr == nullptr) {
            continue;
        }

        SlottedPage slotted_page(*page_ptr);

        for (std::uint16_t i = 0; i < slotted_page.slot_count(); ++i) {
            auto record = slotted_page.record_at(i);
            if (record.has_value() && record->key == key) {
                page_cache_manager_.unpin_page(page_id, false);
                return record;
            }
        }

        page_cache_manager_.unpin_page(page_id, false);
    }

    return std::nullopt;
}

bool HeapFile::delete_record(const RowId& row_id) {
    Page* page_ptr = page_cache_manager_.fetch_page(row_id.page_id);
    if (page_ptr == nullptr) {
        return false;
    }

    SlottedPage slotted_page(*page_ptr);
    if (!slotted_page.delete_record(row_id.slot_index)) {
        page_cache_manager_.unpin_page(row_id.page_id, false);
        return false;
    }

    page_cache_manager_.unpin_page(row_id.page_id, true);
    return true;
}

bool HeapFile::update_record(const RowId& row_id, const Record& record) {
    Page* page_ptr = page_cache_manager_.fetch_page(row_id.page_id);
    if (page_ptr == nullptr) {
        return false;
    }

    SlottedPage slotted_page(*page_ptr);
    if (!slotted_page.update_record(row_id.slot_index, record)) {
        page_cache_manager_.unpin_page(row_id.page_id, false);
        return false;
    }

    page_cache_manager_.unpin_page(row_id.page_id, true);
    return true;
}

std::optional<RowId> HeapFile::update_var_record(const RowId& row_id, const VarRecord& record) {
    Page* page_ptr = page_cache_manager_.fetch_page(row_id.page_id);
    if (page_ptr == nullptr) {
        return std::nullopt;
    }

    SlottedPage slotted_page(*page_ptr);

    const auto* original_slot_entry = slotted_page.slot_at(row_id.slot_index);
    if (original_slot_entry == nullptr) {
        page_cache_manager_.unpin_page(row_id.page_id, false);
        return std::nullopt;
    }

    const std::uint16_t original_length = original_slot_entry->length;

    if (!slotted_page.update_var_record(row_id.slot_index, record)) {
        page_cache_manager_.unpin_page(row_id.page_id, false);
        return std::nullopt;
    }

    std::optional<RowId> result = row_id;

    const auto* updated_slot_entry = slotted_page.slot_at(row_id.slot_index);
    if (updated_slot_entry == nullptr ||
        updated_slot_entry->length != original_length ||
        updated_slot_entry->length == 0) {
        const std::uint16_t new_slot_index =
            static_cast<std::uint16_t>(slotted_page.slot_count() - 1);
        result = RowId{row_id.page_id, new_slot_index};
    }

    page_cache_manager_.unpin_page(row_id.page_id, true);
    return result;
}

}  // namespace db
