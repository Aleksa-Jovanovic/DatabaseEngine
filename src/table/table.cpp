#include "table/table.h"

namespace db::table {

Table::Table(
    const std::string& table_name,
    const std::string& heap_file_name,
    const std::string& index_file_name,
    std::size_t cache_size
)
    : table_name_(table_name),
    heap_file_(heap_file_name),
    primary_index_(index_file_name, cache_size) {}

std::optional<RowId> Table::insert(const Record& record) {
    // First isnert the full row into heap storage to get its physical location.
    const auto row_id = heap_file_.insert(record);
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    // Then index that physical row location by the record's primary key.
    const auto index_result = primary_index_.insert(record.key, row_id.value());
    if (!index_result.has_value()) {
        // For now, leave rollback out of scope and report failure if index insert fails.
        return std::nullopt;
    }

    return row_id;
}

std::optional<Record> Table::get_by_key(std::uint32_t key) {
    // Resolve the key to a physical row location through the primary index first.
    const auto row_id = primary_index_.search(key);
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    // Then fetch the full row payload from the heap storage.
    return heap_file_.get(row_id.value());
}

bool Table::update_by_key(std::uint32_t key, const Record& updated_record) {
    // For the current primary-index design, changing the key would require
    // removing the old index entry and inserting a new one.
    // Keep the first version simple and only allow same-key updates.
    if (updated_record.key != key) {
        return false;
    }

    // Resolve the logical key to a physical row location through the index.
    const auto row_id = primary_index_.search(key);
    if (!row_id.has_value()) {
        return false;
    }

    // Update the heap row in place using the located RowId.
    return heap_file_.update_record(row_id.value(), updated_record);
}

bool Table::delete_by_key(std::uint32_t key) {
    // Delete flow will be implemented later once table-level index maintenance
    // and B+ tree delete behavior are available.
    (void)key;
    return false;
}

}   // namespace db::table