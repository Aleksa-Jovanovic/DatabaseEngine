#pragma once

#include <optional>
#include <string>
#include "storage/cache/page_cache_manager.h"
#include "storage/page/page.h"
#include "storage/page/slotted_page.h"
#include "storage/page/record.h"
#include "storage/page/var_record.h"
#include "common/row_id.h"

namespace db {

// HeapFile stores records in heap-organized pages inside a disk-backed file.
// Records are appended to pages, and lookup is done by sequential scan.
class HeapFile {
public:
    explicit HeapFile(const std::string& file_name);

    // Insert a record into an existing page or a newly allocated page.
    // Returns the physical row location of the inserted record.
    std::optional<RowId> insert(const Record& record);

    // Insert a variable-length record into an existing page or a newly allocated page.
    // Returns the physical row location of the inserted record.
    std::optional<RowId> insert_var_record(const VarRecord& record);

    // Read the fixed-size record at the given row location.
    // Returns std::nullopt if the row id is invalid or deleted.
    std::optional<Record> get(const RowId& row_id);

    // Read the variable-length record at the given row location.
    // Returns std::nullopt if the row id is invalid or deleted.
    std::optional<VarRecord> get_var_record(const RowId& row_id);

    // Scan the heap file and return the first record with the matching key.
    // Returns std::nullopt if no matching record is found.
    std::optional<Record> find(std::uint32_t key);

    // Delete the record at the given row location.
    bool delete_record(const RowId& row_id);

    // Update the fixed-size record at the given row location.
    bool update_record(const RowId& row_id, const Record& record);

    // Update the variable-length record at the given row location.
    // Returns the original RowId for same-size updates or a new RowId if the record moves.
    std::optional<RowId> update_var_record(const RowId& row_id, const VarRecord& record);

private:
    PageCacheManager page_cache_manager_;

    // Try to insert a record into the given page using the slotted-page layout.
    // Returns the inserted slot index on success.
    std::optional<std::uint16_t> try_insert_into_page(Page& page, const Record& record);

    // Try to insert a variable-length record into the given page using the slotted-page layout.
    // Returns the inserted slot index on success.
    std::optional<std::uint16_t> try_insert_var_record_into_page(Page& page, const VarRecord& record);

    // Initialize a freshly allocated heap page as a slotted page.
    void initialize_new_page(Page& page);
};

}  // namespace db
