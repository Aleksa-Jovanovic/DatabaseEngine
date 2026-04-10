#pragma once

#include <optional>
#include <string>
#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"
#include "storage/page/slotted_page.h"
#include "storage/page/record.h"

namespace db {

// HeapFile stores records in heap-organized pages inside a disk-backed file.
// Records are appended to pages, and lookup is done by sequential scan.
class HeapFile {
public:
    explicit HeapFile(const std::string& file_name);

    // Insert a record into an existing page or a newly allocated page.
    void insert(const Record& record);

    // Scan the heap file and return the first record with the matching key.
    // Returns std::nullopt if no matching record is found.
    std::optional<Record> find(std::uint32_t key);

private:
    DiskManager disk_manager_;

    // Try to insert a record into the given page using the slotted-page layout.
    // Returns false if the page does not have enough space.
    bool try_insert_into_page(Page& page, const Record& record);
};

}  // namespace db
