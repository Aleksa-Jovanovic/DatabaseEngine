#include "storage/page/slotted_page.h"

#include <cstring>
#include <vector>
#include "storage/page/var_record_serializer.h"

namespace db {

namespace {
struct LiveRecordImage {
    std::uint16_t slot_index;
    std::uint16_t length;
    std::vector<char> bytes;
};
} // namespace

SlottedPage::SlottedPage(Page& page) : page_(page) {}

void SlottedPage::initialize() {
    auto* header = fetch_header();
    header->slot_count = 0;
    header->free_space_start = sizeof(SlottedPageHeader);
    header->free_space_end = PAGE_SIZE;
}

SlottedPageHeader* SlottedPage::fetch_header() {
    return reinterpret_cast<SlottedPageHeader*>(data());
}

const SlottedPageHeader* SlottedPage::fetch_header() const {
    return reinterpret_cast<const SlottedPageHeader*>(data());
}

SlotEntry *SlottedPage::slot_at(std::uint16_t slot_index) {
    if (slot_index >= fetch_header()->slot_count) {
        return nullptr;
    }

    // Slot entries are stored contiguously right after the slotted-page header.
    auto* slot_entries = reinterpret_cast<SlotEntry*>(data() + sizeof(SlottedPageHeader));
    return &slot_entries[slot_index];
}

const SlotEntry *SlottedPage::slot_at(std::uint16_t slot_index) const {
    if (slot_index >= fetch_header()->slot_count) {
        return nullptr;
    }

    auto* slot_entries = reinterpret_cast<const SlotEntry*>(data() + sizeof(SlottedPageHeader));
    return &slot_entries[slot_index];
}

std::size_t SlottedPage::free_space() const {
    const auto* header = fetch_header();

    if (header->free_space_end < header->free_space_start) {
        return 0;
    }

    return (header->free_space_end - header->free_space_start);
}

bool SlottedPage::insert_record_bytes(const char* record_data, std::uint16_t record_length) {
    if (free_space() < sizeof(SlotEntry) + record_length) {
        return false;
    }

    auto* header = fetch_header();
    const std::uint16_t new_record_offset = header->free_space_end - record_length;

    // Record bytes grow backward from the end of the page.
    std::memcpy(data() + new_record_offset, record_data, record_length);

    auto* new_slot = reinterpret_cast<SlotEntry*>(data() + header->free_space_start);
    new_slot->offset = new_record_offset;
    new_slot->length = record_length;

    header->slot_count += 1;
    header->free_space_start += sizeof(SlotEntry);
    header->free_space_end -= record_length;

    return true;
}

bool SlottedPage::insert_record(const Record& record) {
    const std::uint16_t record_size = static_cast<std::uint16_t>(sizeof(Record));
    return insert_record_bytes(reinterpret_cast<const char*>(&record), record_size);
}

bool SlottedPage::insert_var_record(const VarRecord& record) {
    std::vector<char> serialized_record = serialize_var_record(record);
    const std::uint16_t record_length = static_cast<std::uint16_t>(serialized_record.size());
    return insert_record_bytes(serialized_record.data(), record_length);
}

bool SlottedPage::update_record(std::uint16_t slot_index, const Record &record) {
    auto* slot_entry = slot_at(slot_index);
    if (slot_entry == nullptr) {
        return false;
    }

    if (slot_entry->length != sizeof(Record)) {
        return false;
    }

    if (slot_entry->offset + slot_entry->length > PAGE_SIZE) {
        return false;
    }

    std::memcpy(data() + slot_entry->offset, &record, slot_entry->length);
    return true;
}

bool SlottedPage::update_var_record(std::uint16_t slot_index, const VarRecord &record) {
    auto* slot_entry = slot_at(slot_index);
    if (slot_entry == nullptr) {
        return false;
    }
    
    if (slot_entry->length == 0) {
        return false;
    }

    if(slot_entry->offset + slot_entry->length > PAGE_SIZE) {
        return false;
    }

    std::vector<char> serialized_record = serialize_var_record(record);
    const std::uint16_t new_length = static_cast<std::uint16_t>(serialized_record.size());

    // If the new serialized record has the same size, overwrite in place.
    if (slot_entry->length == new_length) {
        std::memcpy(data() + slot_entry->offset, serialized_record.data(), new_length);
        return true;
    }

    // Safer first-version strategy for size-changing updates:
    // first try to insert the new record, then delete the old slot only if
    // the new insert succeeds.
    if (!insert_var_record(record)) {
        return false;
    }

    return delete_record(slot_index);
}

bool SlottedPage::delete_record(std::uint16_t slot_index) {
    auto* slot_entry = slot_at(slot_index);
    if (slot_entry == nullptr) {
        return false;
    }

    if (slot_entry->length == 0) {
        return false;
    }

    slot_entry->length = 0;
    return true;
}

// Compaction keeps the same number of slot entries and preserves slot indexes.
// That is important because RowId depends on stable slot positions.
void SlottedPage::compact_page() {
    std::vector<LiveRecordImage> live_records;

    for (std::uint16_t i = 0; i < slot_count(); ++i) {
        const auto* slot_entry = slot_at(i);
        if (slot_entry == nullptr) {
            continue;
        }

        if (slot_entry->length == 0) {
            continue;
        }

        if (slot_entry->offset + slot_entry->length > PAGE_SIZE) {
            continue;
        }

        LiveRecordImage record_image{};
        record_image.slot_index = i;
        record_image.length = slot_entry->length;
        record_image.bytes.resize(slot_entry->length);

        // Copy live record bytes out first so compaction does not depend on
        // overlapping in-place moves inside the same page buffer.
        std::memcpy(
            record_image.bytes.data(),
            data() + slot_entry->offset,
            slot_entry->length
        );

        live_records.push_back(std::move(record_image));
    }

    std::uint16_t write_pointer = PAGE_SIZE;

    for (const auto& record_image : live_records) {
        // Pack live records tightly from the end of the page backward, leaving
        // one contiguous free-space region between the slot directory and data.
        write_pointer -= record_image.length;

        std::memcpy(
            data() + write_pointer,
            record_image.bytes.data(),
            record_image.length
        );

        auto* slot_entry = slot_at(record_image.slot_index);
        slot_entry->offset = write_pointer;
    }

    // Slot count and slot directory size stay unchanged, so only the end of
    // the free-space region moves after compaction.
    fetch_header()->free_space_end = write_pointer;
}

std::optional<Record> SlottedPage::record_at(std::uint16_t slot_index) const {
    const auto* slot = slot_at(slot_index);
    if (slot == nullptr) {
        return std::nullopt;
    }

    if (slot->length != sizeof(Record)) {
        return std::nullopt;
    }

    if (slot->offset + slot->length > PAGE_SIZE) {
        return std::nullopt;
    }

    Record record{};
    std::memcpy(&record, data() + slot->offset, sizeof(Record));
    return record;
}

std::optional<VarRecord> SlottedPage::var_record_at(std::uint16_t slot_index) const {
    const auto* slot_entry = slot_at(slot_index);
    if (slot_entry == nullptr) {
        return std::nullopt;
    }
    
    if (slot_entry->offset + slot_entry->length > PAGE_SIZE) {
        return std::nullopt;
    }

    std::vector<char> buffer(slot_entry->length);
    std::memcpy(buffer.data(), data() + slot_entry->offset, slot_entry->length);

    std::optional<VarRecord> record = deserialize_var_record(buffer.data(), slot_entry->length);
    return record;
}

std::uint16_t SlottedPage::slot_count() const {
    return fetch_header()->slot_count;
}

char* SlottedPage::data() {
    return page_.data.data();
}

const char* SlottedPage::data() const {
    return page_.data.data();
}

std::size_t SlottedPage::required_space_for_record() const {
    return sizeof(SlotEntry) + sizeof(Record);
}

}  // namespace db
