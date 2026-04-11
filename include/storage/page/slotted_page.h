#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "storage/page/page.h"
#include "storage/page/page_layout.h"
#include "storage/page/record.h"
#include "storage/page/var_record.h"

namespace db {

// SlottedPage manages the byte layout of one slotted page in memory.
// It does not perform disk I/O.
// It only knows how records and slots are organized inside Page::data.
class SlottedPage {
public:
    explicit SlottedPage(Page& page);

    // Initialize a fresh page with an empty slotted-page layout.
    void initialize();

    // Return the current header.
    SlottedPageHeader* fetch_header();
    const SlottedPageHeader* fetch_header() const;

    // Return the slot entry at the given index.
    SlotEntry* slot_at(std::uint16_t slot_index);
    const SlotEntry* slot_at(std::uint16_t slot_index) const;

    // Return how many free bytes remain between slot directory and record area.
    std::size_t free_space() const;

    // Insert one fixed-size record into the page.
    // Returns false if the page does not have enough free space.
    bool insert_record(const Record& record);

    // Insert one variable-length record into the page.
    // Returns false if the page does not have enough free space.
    bool insert_var_record(const VarRecord& record);

    // Update one fixed-size record in place.
    // Returns false if the slot is invalid, deleted, or not a fixed-size Record slot.
    bool update_record(std::uint16_t slot_index, const Record& record);

    // Update one variable-length record.
    // If the new serialized record has the same size, overwrite in place.
    // Otherwise, delete the old slot logically and insert the new record into a new slot.
    bool update_var_record(std::uint16_t slot_index, const VarRecord& record);

    // Delete one record at slot index by invalidating the length
    // Returns false if the slot index is invalid
    bool delete_record(std::uint16_t slot_index);

    // Compact live record bytes toward the end of the page and rebuild one
    // contiguous free-space region in the middle.
    void compact_page();

    // Read the record referenced by the given slot.
    // Returns std::nullopt if the slot index is invalid.
    std::optional<Record> record_at(std::uint16_t slot_index) const;
    std::optional<VarRecord> var_record_at(std::uint16_t slot_index) const;

    // Return the number of slots currently stored in the page.
    std::uint16_t slot_count() const;

private:
    Page& page_;

    // Return a pointer to the start of the raw page bytes.
    char* data();
    const char* data() const;

    // Return how many bytes are needed to add one more slot and one record.
    std::size_t required_space_for_record() const;

    // Insert one raw record payload and create a matching slot entry.
    bool insert_record_bytes(const char* record_data, std::uint16_t record_length);
};

}   // namespace db
