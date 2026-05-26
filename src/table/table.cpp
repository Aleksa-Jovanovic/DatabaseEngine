#include "table/table.h"

namespace db::table {

Table::Table(
    const std::string& table_name,
    const std::string& heap_file_name,
    const std::string& index_file_name,
    std::size_t cache_size
) : Table(TableMetadata{
        table_name,
        heap_file_name,
        index_file_name,
        cache_size
    }) {}

Table::Table(const TableMetadata& metadata)
    : metadata_(metadata),
      heap_file_(metadata.heap_file_name, metadata.cache_size),
      primary_index_(metadata.primary_index_file_name, metadata.cache_size) {}

std::optional<RowId> Table::insert(const Row& row) {
    // Bridge the logical table row into the current variable-length heap record shape.
    VarRecord record{row.key, row.value};

    // First insert the full row into heap storage to get its physical location.
    const auto row_id = heap_file_.insert_var_record(record);
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    // Then index that physical row location by the row's primary key.
    const auto index_result = primary_index_.insert(row.key, row_id.value());
    if (!index_result.has_value()) {
        // For now, leave rollback out of scope and report failure if index insert fails.
        return std::nullopt;
    }

    return row_id;
}

std::optional<Row> Table::get_by_key(std::uint32_t key) {
    // Resolve the key to a physical row location through the primary index first.
    const auto row_id = primary_index_.search(key);
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    // Fetch the stored variable-length row payload from heap storage.
    const auto record = heap_file_.get_var_record(row_id.value());
    if (!record.has_value()) {
        return std::nullopt;
    }

    // Convert the heap record back into the logical table row type.
    return Row{record->key, record->value};
}

bool Table::update_by_key(std::uint32_t key, const Row& updated_row) {
    // For the current primary-index design, changing the key would require
    // removing the old index entry and inserting a new one.
    // Keep the first version simple and only allow same-key updates.
    if (updated_row.key != key) {
        return false;
    }

    // Resolve the logical key to a physical row location through the index.
    const auto row_id = primary_index_.search(key);
    if (!row_id.has_value()) {
        return false;
    }

    VarRecord updated_record{updated_row.key, updated_row.value};

    // Update the heap row using the located RowId.
    const auto updated_row_id =
        heap_file_.update_var_record(row_id.value(), updated_record);

    if (!updated_row_id.has_value()) {
        return false;
    }

    // If the variable-length update moved the row, repair the index mapping.
    if (updated_row_id.value().page_id != row_id->page_id ||
        updated_row_id.value().slot_index != row_id->slot_index) {
        const auto index_update_result =
            primary_index_.update(key, updated_row_id.value());
        if (!index_update_result.has_value()) {
            return false;
        }
    }

    return true;
}

bool Table::delete_by_key(std::uint32_t key) {
    // Delete flow will be implemented later once table-level index maintenance
    // and B+ tree delete behavior are available.
    (void)key;
    return false;
}

bool Table::add_secondary_index(
    const std::string& index_name,
    const std::string& column_name,
    const std::string& index_file_name
) {
    if (index_name.empty() || column_name.empty() || index_file_name.empty()) {
        return false;
    }

    // Do not allow duplicate secondary-index names.
    if (secondary_indexes_.find(index_name) != secondary_indexes_.end()) {
        return false;
    }

    // Keep the first version narrow and only recognize the current table-row columns.
    if (column_name != "key" && column_name != "value") {
        return false;
    }

    // The current primary index already owns the primary-key path, so keep
    // secondary registration focused on non-primary indexes.
    auto tree = std::make_unique<index::BPlusTree>(
        index_file_name,
        metadata_.cache_size
    );

    SecondaryIndexInfo info{
        index_name,
        column_name,
        index_file_name,
        true,
        std::move(tree)
    };

    secondary_indexes_.emplace(index_name, std::move(info));

    metadata_.secondary_indexes.push_back(IndexMetadata{
        index_name,
        column_name,
        index_file_name,
        false,
        true
    });

    return true;
}

}  // namespace db::table
