#include "catalog/catalog.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "catalog/catalog_serializer.h"
#include "index/bplustree.h"

namespace db::catalog {

namespace {

table::ColumnType to_table_column_type(ColumnType type) {
    switch (type) {
        case ColumnType::Integer:
            return table::ColumnType::Integer;
        case ColumnType::String:
            return table::ColumnType::String;
        case ColumnType::Boolean:
            return table::ColumnType::Boolean;
        case ColumnType::Date:
            return table::ColumnType::Date;
    }

    return table::ColumnType::String;
}

bool remove_file_if_exists(const std::string& file_name) {
    if (file_name.empty()) {
        return false;
    }

    std::error_code error;
    const bool removed = std::filesystem::remove(file_name, error);

    if (error) {
        return false;
    }

    // If the file did not exist, remove(...) returns false without an error.
    // That is okay for DROP TABLE because catalog metadata is the source of truth.
    (void)removed;
    return true;
}

void remove_empty_table_directory(const TableDefinition& table_definition) {
    const std::filesystem::path table_directory =
        std::filesystem::path(table_definition.heap_file_name).parent_path();

    // Flat legacy/test file names have no table-local directory to remove.
    if (table_directory.empty() || table_directory == ".") {
        return;
    }

    for (const IndexDefinition& index_definition : table_definition.indexes) {
        if (std::filesystem::path(index_definition.file_name).parent_path() !=
            table_directory) {
            return;
        }
    }

    // Only remove the directory when it is empty. This is intentionally best
    // effort so an unrelated file or filesystem error cannot undo the DROP.
    std::error_code error;
    std::filesystem::remove(table_directory, error);
}

bool remove_table_files(const TableDefinition& table_definition) {
    if (!remove_file_if_exists(table_definition.heap_file_name)) {
        return false;
    }

    for (const IndexDefinition& index_definition : table_definition.indexes) {
        if (!remove_file_if_exists(index_definition.file_name)) {
            return false;
        }
    }

    remove_empty_table_directory(table_definition);
    return true;
}

bool column_exists_with_type(
    const TableDefinition& table_definition,
    const std::string& column_name,
    ColumnType expected_type,
    bool allow_primary_key
) {
    for (const ColumnDefinition& column : table_definition.schema.columns()) {
        if (column.name != column_name) {
            continue;
        }

        if (!allow_primary_key && column.is_primary_key) {
            return false;
        }

        return column.type == expected_type;
    }

    return false;
}

bool catalog_has_index_name(
    const CatalogMetadata& metadata,
    const std::string& index_name
) {
    for (const TableDefinition& table_definition : metadata.tables) {
        for (const IndexDefinition& index_definition : table_definition.indexes) {
            if (index_definition.index_name == index_name) {
                return true;
            }
        }
    }

    return false;
}

}  // namespace

Catalog::Catalog(const CatalogMetadata& metadata) : metadata_(metadata) {}

Catalog::Catalog(const std::string& metadata_file_name)
    : metadata_file_name_(metadata_file_name) {
    load_from_file();
}

bool Catalog::create_table(const TableDefinition& table_definition) {
    if (!validate_table_definition(table_definition)) {
        return false;
    }

    if (has_table(table_definition.table_name)) {
        return false;
    }

    // Bootstrap the physical table artifacts before making the catalog entry durable.
    const auto table_metadata = build_table_metadata_from_definition(table_definition);
    if (!table_metadata.has_value()) {
        return false;
    }

    // Constructing the runtime table eagerly opens or creates the heap and
    // index files described by the metadata.
    table::Table bootstrap_table(table_metadata.value());

    metadata_.tables.push_back(table_definition);

    // File-backed catalogs should persist table-definition changes immediately.
    if (!metadata_file_name_.empty() && !save_to_file()) {
        metadata_.tables.pop_back();
        return false;
    }

    return true;
}

bool Catalog::drop_table(const std::string& table_name) {
    if (table_name.empty()) {
        return false;
    }

    for (auto table_it = metadata_.tables.begin();
         table_it != metadata_.tables.end();
         ++table_it) {
        if (table_it->table_name != table_name) {
            continue;
        }

        const TableDefinition table_definition = *table_it;

        if (!remove_table_files(table_definition)) {
            return false;
        }

        metadata_.tables.erase(table_it);

        if (!metadata_file_name_.empty() && !save_to_file()) {
            metadata_.tables.push_back(table_definition);
            return false;
        }

        return true;
    }

    return false;
}

bool Catalog::create_index(const IndexDefinition& index_definition) {
    if (index_definition.index_name.empty() ||
        index_definition.table_name.empty() ||
        index_definition.column_name.empty() ||
        index_definition.file_name.empty()) {
        return false;
    }

    if (index_definition.is_primary) {
        return false;
    }

    if (catalog_has_index_name(metadata_, index_definition.index_name)) {
        return false;
    }

    for (TableDefinition& table_definition : metadata_.tables) {
        if (table_definition.table_name != index_definition.table_name) {
            continue;
        }

        // Secondary indexes currently support integer columns only. Duplicate indexed
        // values are handled by encoding (indexed_value, primary_key) into the B+ tree key.
        if (!column_exists_with_type(
                table_definition,
                index_definition.column_name,
                ColumnType::Integer,
                false
            )) {
            return false;
        }

        index::BPlusTree bootstrap_index(index_definition.file_name);

        table_definition.indexes.push_back(index_definition);

        if (!metadata_file_name_.empty() && !save_to_file()) {
            table_definition.indexes.pop_back();
            remove_file_if_exists(index_definition.file_name);
            return false;
        }

        return true;
    }

    return false;
}

bool Catalog::drop_index(const std::string& index_name) {
    if (index_name.empty()) {
        return false;
    }

    for (TableDefinition& table_definition : metadata_.tables) {
        for (auto index_it = table_definition.indexes.begin();
             index_it != table_definition.indexes.end();
             ++index_it) {
            if (index_it->index_name != index_name) {
                continue;
            }

            if (index_it->is_primary) {
                return false;
            }

            const IndexDefinition index_definition = *index_it;
            const auto restore_position =
                std::distance(table_definition.indexes.begin(), index_it);

            if (!remove_file_if_exists(index_definition.file_name)) {
                return false;
            }

            table_definition.indexes.erase(index_it);

            if (!metadata_file_name_.empty() && !save_to_file()) {
                table_definition.indexes.insert(
                    table_definition.indexes.begin() + restore_position,
                    index_definition
                );
                return false;
            }

            return true;
        }
    }

    return false;
}

bool Catalog::has_table(const std::string& table_name) const {
    for (const TableDefinition& table_definition : metadata_.tables) {
        if (table_definition.table_name == table_name) {
            return true;
        }
    }

    return false;
}

std::optional<TableDefinition> Catalog::find_table_definition(const std::string& table_name) const {
    for (const TableDefinition& table_definition : metadata_.tables) {
        if (table_definition.table_name == table_name) {
            return table_definition;
        }
    }

    return std::nullopt;
}

std::optional<table::TableMetadata> Catalog::build_table_metadata_from_definition(
        const TableDefinition& table_definition,
        std::size_t cache_size
) const {
    std::string primary_index_file_name;
    std::vector<table::IndexMetadata> secondary_indexes;
    std::vector<table::ColumnMetadata> columns;

    for (const ColumnDefinition& column : table_definition.schema.columns()) {
        columns.push_back(table::ColumnMetadata{
            column.name,
            to_table_column_type(column.type),
            column.is_primary_key,
            column.is_auto_increment
        });
    }

    // Catalog keeps all indexes in one list. Runtime TableMetadata still
    // expects one explicit primary index plus a list of secondary indexes.
    for (const IndexDefinition& index_definition : table_definition.indexes) {
        if (index_definition.is_primary) {
            // Reject ambiguous catalog state with more than one primary index.
            if (!primary_index_file_name.empty()) {
                return std::nullopt;
            }

            primary_index_file_name = index_definition.file_name;
            continue;
        }

        secondary_indexes.push_back(table::IndexMetadata{
            index_definition.index_name,
            index_definition.column_name,
            index_definition.file_name,
            index_definition.is_primary,
            index_definition.is_unique
        });
    }

    // A table must have exactly one primary index before it can be rebuilt
    // into runtime table metadata.
    if (primary_index_file_name.empty()) {
        return std::nullopt;
    }

    return table::TableMetadata{
        table_definition.table_name,
        table_definition.heap_file_name,
        primary_index_file_name,
        cache_size,
        std::move(columns),
        std::move(secondary_indexes)
    };
}

std::optional<table::TableMetadata> Catalog::build_table_metadata(
    const std::string& table_name,
    std::size_t cache_size
) const {
    const auto table_definition = find_table_definition(table_name);
    if (!table_definition.has_value()) {
        return std::nullopt;
    }

    return build_table_metadata_from_definition(table_definition.value(), cache_size);
}

std::unique_ptr<table::Table> Catalog::open_table(
    const std::string& table_name,
    std::size_t cache_size
) const {
    const auto table_metadata = build_table_metadata(table_name, cache_size);
    if (!table_metadata.has_value()) {
        return nullptr;
    }

    return std::make_unique<table::Table>(table_metadata.value());
}

const CatalogMetadata& Catalog::metadata() const {
    return metadata_;
}

bool Catalog::validate_table_definition(const TableDefinition& table_definition) const {
    if (table_definition.table_name.empty() || table_definition.heap_file_name.empty()) {
        return false;
    }

    const auto& columns = table_definition.schema.columns();
    if (columns.empty()) {
        return false;
    }

    std::unordered_set<std::string> column_names;
    std::string primary_key_column_name;
    ColumnType primary_key_column_type = ColumnType::Integer;
    std::size_t primary_key_count = 0;

    for (const ColumnDefinition& column : columns) {
        if (column.name.empty()) {
            return false;
        }

        if (!column_names.insert(column.name).second) {
            return false;
        }

        if (column.is_primary_key) {
            ++primary_key_count;

            if (primary_key_count > 1) {
                return false;
            }

            primary_key_column_name = column.name;
            primary_key_column_type = column.type;
        }

        if (column.is_auto_increment && !column.is_primary_key) {
            return false;
        }

        if (column.is_auto_increment && column.type != ColumnType::Integer) {
            return false;
        }
    }

    if (primary_key_count != 1) {
        return false;
    }

    // The current table and B+ tree implementation only supports integer primary keys.
    if (primary_key_column_type != ColumnType::Integer) {
        return false;
    }

    if (table_definition.indexes.empty()) {
        return false;
    }

    std::unordered_set<std::string> index_names;
    std::size_t primary_index_count = 0;
    std::string primary_index_column_name;

    for (const IndexDefinition& index_definition : table_definition.indexes) {
        if (index_definition.index_name.empty() ||
            index_definition.table_name.empty() ||
            index_definition.column_name.empty() ||
            index_definition.file_name.empty()) {
            return false;
        }

        if (index_definition.table_name != table_definition.table_name) {
            return false;
        }

        if (!index_names.insert(index_definition.index_name).second) {
            return false;
        }

        if (column_names.find(index_definition.column_name) == column_names.end()) {
            return false;
        }

        if (index_definition.is_primary) {
            ++primary_index_count;

            if (primary_index_count > 1) {
                return false;
            }

            primary_index_column_name = index_definition.column_name;
        }
    }

    if (primary_index_count != 1) {
        return false;
    }

    if (primary_index_column_name != primary_key_column_name) {
        return false;
    }

    return true;
}

bool Catalog::load_from_file() {
    if (metadata_file_name_.empty()) {
        return false;
    }

    std::ifstream input(metadata_file_name_, std::ios::binary);
    if (!input.is_open()) {
        // Missing file is treated as an empty catalog, not as a hard failure.
        metadata_ = CatalogMetadata{};
        return true;
    }

    // Measure the file first so we can allocate one exact-size buffer.
    input.seekg(0, std::ios::end);
    const std::streamoff file_size = input.tellg();
    input.seekg(0, std::ios::beg);

    if (file_size < 0) {
        return false;
    }

    std::vector<char> buffer(static_cast<std::size_t>(file_size));
    if (!buffer.empty()) {
        // Read the full catalog blob in one shot before deserializing it.
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (!input) {
            return false;
        }
    }

    const auto decoded = CatalogSerializer::deserialize(buffer.data(), buffer.size());
    if (!decoded.has_value()) {
        return false;
    }

    metadata_ = decoded.value();
    return true;
}

bool Catalog::save_to_file() const {
    if (metadata_file_name_.empty()) {
        return false;
    }

    // Persist the full current catalog state as one serialized blob.
    const std::vector<char> buffer = CatalogSerializer::serialize(metadata_);

    std::ofstream output(metadata_file_name_, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    if (!buffer.empty()) {
        output.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (!output) {
            return false;
        }
    }

    return true;
}

}  // namespace db::catalog
