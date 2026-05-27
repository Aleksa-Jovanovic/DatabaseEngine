#include "catalog/catalog.h"

#include <memory>
#include <utility>

namespace db::catalog {

Catalog::Catalog(const CatalogMetadata& metadata) : metadata_(metadata) {}

bool Catalog::create_table(const TableDefinition& table_definition) {
    if (table_definition.table_name.empty()) {
        return false;
    }

    if (has_table(table_definition.table_name)) {
        return false;
    }

    metadata_.tables.push_back(table_definition);
    return true;
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

std::optional<table::TableMetadata> Catalog::build_table_metadata(
    const std::string& table_name,
    std::size_t cache_size
) const {
    const auto table_definition = find_table_definition(table_name);
    if (!table_definition.has_value()) {
        return std::nullopt;
    }

    std::string primary_index_file_name;
    std::vector<table::IndexMetadata> secondary_indexes;

    for (const IndexDefinition& index_definition : table_definition->indexes) {
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
        table_definition->table_name,
        table_definition->heap_file_name,
        primary_index_file_name,
        cache_size,
        std::move(secondary_indexes)
    };
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

}  // namespace db::catalog