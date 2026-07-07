#include "table/table.h"

#include "index/index_key.h"
#include "table/row_serializer.h"

#include <algorithm>
#include <string>
#include <vector>

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
      primary_index_(metadata.primary_index_file_name, metadata.cache_size) {
    load_secondary_indexes_from_metadata();
    initialize_next_primary_key_value();
}

std::optional<RowId> Table::insert(const Row& row) {
    if (!validate_row(row)) {
        return std::nullopt;
    }

    const std::vector<char> serialized_row = RowSerializer::serialize(row);
    const std::string payload(serialized_row.begin(), serialized_row.end());

    VarRecord record{row.key, payload};

    const auto row_id = heap_file_.insert_var_record(record);
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    const auto index_result = primary_index_.insert(
        index::encode_primary_key(row.key), 
        row_id.value()
    );
    if (!index_result.has_value()) {
        // Keep heap scans consistent with indexed lookups if primary-index
        // insertion fails after the row was already written.
        heap_file_.delete_record(row_id.value());
        return std::nullopt;
    }

    if (!insert_secondary_index_entries(row, row_id.value())) {
        primary_index_.delete_key(index::encode_primary_key(row.key));
        heap_file_.delete_record(row_id.value());
        return std::nullopt;
    }

    advance_next_primary_key_value(row.key);

    return row_id;
}

std::optional<Row> Table::get_by_key(std::uint32_t key) {
    // Resolve the key to a physical row location through the primary index first.
    const auto row_id = primary_index_.search(index::encode_primary_key(key));
    if (!row_id.has_value()) {
        return std::nullopt;
    }

    // Fetch the stored variable-length row payload from heap storage.
    const auto record = heap_file_.get_var_record(row_id.value());
    if (!record.has_value()) {
        return std::nullopt;
    }

    // Convert the heap record back into the logical table row type.
    return RowSerializer::deserialize(
        record->key,
        record->value.data(),
        record->value.size()
    );
}

std::vector<Row> Table::scan() {
    std::vector<Row> rows;

    const auto records = heap_file_.scan_var_records();
    for (const auto& record_entry : records) {
        const auto& record = record_entry.second;

        auto row = RowSerializer::deserialize(
            record.key,
            record.value.data(),
            record.value.size()
        );

        if (!row.has_value()) {
            continue;
        }

        rows.push_back(row.value());
    }

    return rows;
}

bool Table::update_by_key(std::uint32_t key, const Row& updated_row) {
    if (!validate_row(updated_row)) {
        return false;
    }

    // For the current primary-index design, changing the key would require
    // removing the old index entry and inserting a new one.
    // Keep the first version simple and only allow same-key updates.
    if (updated_row.key != key) {
        return false;
    }

    // Resolve the logical key to a physical row location through the index.
    const auto row_id = primary_index_.search(index::encode_primary_key(key));
    if (!row_id.has_value()) {
        return false;
    }

    const auto old_record = heap_file_.get_var_record(row_id.value());
    if (!old_record.has_value()) {
        return false;
    }

    const auto old_row = RowSerializer::deserialize(
        old_record->key,
        old_record->value.data(),
        old_record->value.size()
    );

    if (!old_row.has_value()) {
        return false;
    }

    const std::vector<char> serialized_row = RowSerializer::serialize(updated_row);
    const std::string payload(serialized_row.begin(), serialized_row.end());

    VarRecord updated_record{updated_row.key, payload};

    // Update the heap row using the located RowId.
    const auto updated_row_id =
        heap_file_.update_var_record(row_id.value(), updated_record);

    if (!updated_row_id.has_value()) {
        return false;
    }

    // If the variable-length update moved the row, repair the index mapping.
    if (updated_row_id.value().page_id != row_id->page_id ||
        updated_row_id.value().slot_index != row_id->slot_index) {
        const auto index_update_result = primary_index_.update(
            index::encode_primary_key(key),
            updated_row_id.value()
        );

        if (!index_update_result.has_value()) {
            return false;
        }
    }

    if (!update_secondary_index_entries(
            old_row.value(),
            updated_row,
            updated_row_id.value()
        )) {
        return false;
    }

    return true;
}

// If heap delete succeeds and index delete fails, we currently cannot roll back the heap delete.
// In this version, that would leave inconsistent state.
// To reduce risk, delete from index first, then heap.
// This has the opposite risk: if index delete succeeds and heap delete fails,
// the row becomes unreachable by index but may still appear in full heap scan.
bool Table::delete_by_key(std::uint32_t key) {
    const auto row_id = primary_index_.search(index::encode_primary_key(key));
    if (!row_id.has_value()) {
        return false;
    }

    // We first need to fetch/deserialise the row using the RowId before deleting anything
    const auto record = heap_file_.get_var_record(row_id.value());
    if (!record.has_value()) {
        return false;
    }

    const auto row = RowSerializer::deserialize(
        record->key,
        record->value.data(),
        record->value.size()
    );
    if (!row.has_value()) {
        return false;
    }

    if (!delete_secondary_index_entries(row.value())) {
        return false;
    }

    const auto deleted_index_entry = primary_index_.delete_key(index::encode_primary_key(key));
    if (!deleted_index_entry.has_value()) {
        return false;
    }

    if (!heap_file_.delete_record(row_id.value())) {
        return false;
    }

    return true;
}

std::uint32_t Table::allocate_primary_key() {
    const std::uint32_t key = next_primary_key_value_;
    ++next_primary_key_value_;
    return key;
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

    // Keep registration forward-compatible. The current engine does not yet
    // maintain arbitrary secondary indexes, but metadata can still describe them.

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

void Table::load_secondary_indexes_from_metadata() {
    secondary_indexes_.clear();

    for (const IndexMetadata& index_metadata : metadata_.secondary_indexes) {
        if (index_metadata.index_name.empty() || index_metadata.file_name.empty()) {
            continue;
        }

        // Ignore duplicate metadata entries so runtime index state stays
        // one-to-one by index name.
        if (secondary_indexes_.find(index_metadata.index_name) != secondary_indexes_.end()) {
            continue;
        }

        // Keep metadata forward-compatible: allow any column name here even if
        // the current engine cannot actively maintain or use that index yet.
        auto tree = std::make_unique<index::BPlusTree>(
            index_metadata.file_name,
            metadata_.cache_size
        );

        SecondaryIndexInfo info{
            index_metadata.index_name,
            index_metadata.column_name,
            index_metadata.file_name,
            index_metadata.is_unique,
            std::move(tree)
        };

        secondary_indexes_.emplace(index_metadata.index_name, std::move(info));
    }
}

bool Table::validate_row(const Row& row) const {
    if (metadata_.columns.empty()) {
        return true;
    }

    if (row.values.size() != metadata_.columns.size()) {
        return false;
    }

    for (std::size_t i = 0; i < metadata_.columns.size(); ++i) {
        const ColumnMetadata& column = metadata_.columns[i];
        const FieldValue& value = row.values[i];

        switch (column.type) {
            case ColumnType::Integer:
                if (!std::holds_alternative<std::int64_t>(value)) {
                    return false;
                }
                break;

            case ColumnType::String:
                if (!std::holds_alternative<std::string>(value)) {
                    return false;
                }
                break;

            case ColumnType::Boolean:
                if (!std::holds_alternative<bool>(value)) {
                    return false;
                }
                break;

            case ColumnType::Date:
                if (!std::holds_alternative<DateValue>(value)) {
                    return false;
                }
                break;
        }

        if (column.is_primary_key) {
            if (!std::holds_alternative<std::int64_t>(value)) {
                return false;
            }

            if (std::get<std::int64_t>(value) != static_cast<std::int64_t>(row.key)) {
                return false;
            }
        }
    }

    return true;
}

void Table::advance_next_primary_key_value(std::uint32_t key) {
    if (key >= next_primary_key_value_) {
        next_primary_key_value_ = key + 1;
    }
}

// Heap scan order is physical insertion/storage order, not primary-key order,
// so rebuild the live auto-increment counter from the maximum existing key.
void Table::initialize_next_primary_key_value() {
    next_primary_key_value_ = 1;

    const std::vector<Row> rows = scan();
    for (const Row& row : rows) {
        advance_next_primary_key_value(row.key);
    }
}

std::optional<index::IndexKey> Table::encode_secondary_key_for_row(
    const Row& row,
    const std::string& column_name
) const {
    const auto column_it = std::find_if(
        metadata_.columns.begin(),
        metadata_.columns.end(),
        [&](const auto& column) {
            return column.name == column_name;
        }
    );

    if (column_it == metadata_.columns.end()) {
        return std::nullopt;
    }

    const std::size_t column_index =
        static_cast<std::size_t>(std::distance(metadata_.columns.begin(), column_it));

    if (column_index >= row.values.size()) {
        return std::nullopt;
    }

    const auto* indexed_value = std::get_if<std::int64_t>(&row.values[column_index]);
    if (indexed_value == nullptr) {
        return std::nullopt;
    }

    return index::encode_secondary_integer_key(*indexed_value, row.key);
}

bool Table::insert_secondary_index_entries(
    const Row& row,
    const RowId& row_id
) {
    std::vector<std::pair<std::string, index::IndexKey>> inserted_keys;

    for (auto& secondary_index_entry : secondary_indexes_) {
        const std::string& index_name = secondary_index_entry.first;
        SecondaryIndexInfo& index_info = secondary_index_entry.second;

        const auto encoded_key = encode_secondary_key_for_row(
            row,
            index_info.column_name
        );

        if (!encoded_key.has_value()) {
            for (const auto& inserted_key : inserted_keys) {
                auto it = secondary_indexes_.find(inserted_key.first);
                if (it != secondary_indexes_.end()) {
                    it->second.tree->delete_key(inserted_key.second);
                }
            }

            return false;
        }

        const auto inserted_index_entry = index_info.tree->insert(
            encoded_key.value(),
            row_id
        );

        if (!inserted_index_entry.has_value()) {
            for (const auto& inserted_key : inserted_keys) {
                auto it = secondary_indexes_.find(inserted_key.first);
                if (it != secondary_indexes_.end()) {
                    it->second.tree->delete_key(inserted_key.second);
                }
            }

            return false;
        }

        inserted_keys.push_back({index_name, encoded_key.value()});
    }

    return true;
}

bool Table::delete_secondary_index_entries(const Row& row) {
    for (auto& secondary_index_entry : secondary_indexes_) {
        SecondaryIndexInfo& index_info = secondary_index_entry.second;

        const auto encoded_key = encode_secondary_key_for_row(
            row,
            index_info.column_name
        );

        if (!encoded_key.has_value()) {
            return false;
        }

        const auto deleted_entry = index_info.tree->delete_key(encoded_key.value());
        if (!deleted_entry.has_value()) {
            return false;
        }
    }

    return true;
}

bool Table::update_secondary_index_entries(
    const Row& old_row,
    const Row& new_row,
    const RowId& new_row_id
) {
    for (auto& secondary_index_entry : secondary_indexes_) {
        SecondaryIndexInfo& index_info = secondary_index_entry.second;

        const auto old_encoded_key = encode_secondary_key_for_row(
            old_row,
            index_info.column_name
        );
        const auto new_encoded_key = encode_secondary_key_for_row(
            new_row,
            index_info.column_name
        );

        if (!old_encoded_key.has_value() || !new_encoded_key.has_value()) {
            return false;
        }

        // Secondary index keys include the indexed value, so changing that
        // value requires moving the entry instead of only updating its RowId.
        if (old_encoded_key.value() == new_encoded_key.value()) {
            const auto updated_entry = index_info.tree->update(
                old_encoded_key.value(),
                new_row_id
            );

            if (!updated_entry.has_value()) {
                return false;
            }

            continue;
        }

        const auto deleted_entry = index_info.tree->delete_key(
            old_encoded_key.value()
        );
        if (!deleted_entry.has_value()) {
            return false;
        }

        const auto inserted_entry = index_info.tree->insert(
            new_encoded_key.value(),
            new_row_id
        );
        if (!inserted_entry.has_value()) {
            return false;
        }
    }

    return true;
}

}  // namespace db::table
