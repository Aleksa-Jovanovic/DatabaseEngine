#include "catalog/catalog.h"

#include <fstream>
#include <memory>
#include <utility>
#include <vector>

#include "catalog/catalog_serializer.h"

namespace db::catalog {

Catalog::Catalog(const CatalogMetadata& metadata) : metadata_(metadata) {}

Catalog::Catalog(const std::string& metadata_file_name)
    : metadata_file_name_(metadata_file_name) {
    load_from_file();
}

bool Catalog::create_table(const TableDefinition& table_definition) {
    if (table_definition.table_name.empty()) {
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
        std::size_t cache_size = 8
) const {
    std::string primary_index_file_name;
    std::vector<table::IndexMetadata> secondary_indexes;

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
