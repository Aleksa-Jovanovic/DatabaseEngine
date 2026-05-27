#pragma once

#include <memory>
#include <optional>
#include <string>

#include "catalog/catalog_metadata.h"
#include "table/table.h"
#include "table/table_metadata.h"

namespace db::catalog {

// Catalog is the database-wide metadata registry. It will eventually track
// what tables exist, what schemas they have, and what indexes belong to them,
// so higher layers can discover database objects instead of hardcoding them.
class Catalog {
public:
    Catalog() = default;
    explicit Catalog(const CatalogMetadata& metadata);
    // When a metadata file is provided, Catalog will try to rebuild its
    // in-memory registry from that file on construction.
    explicit Catalog(const std::string& metadata_file_name);

    bool create_table(const TableDefinition& table_definition);
    bool has_table(const std::string& table_name) const;
    std::optional<TableDefinition> find_table_definition(const std::string& table_name) const;

    std::unique_ptr<table::Table> open_table(
        const std::string& table_name,
        std::size_t cache_size = 8
    ) const;

    const CatalogMetadata& metadata() const;

#ifdef DB_TESTING
    // Exposed for direct conversion-path testing; otherwise kept private as
    // a catalog implementation detail behind open_table(...).
    std::optional<table::TableMetadata> build_table_metadata(
        const std::string& table_name,
        std::size_t cache_size = 8
    ) const;
#endif

private:
    CatalogMetadata metadata_;
    // Empty means this Catalog instance is purely in-memory for now.
    std::string metadata_file_name_;

#ifndef DB_TESTING
    std::optional<table::TableMetadata> build_table_metadata(
        const std::string& table_name,
        std::size_t cache_size = 8
    ) const;
#endif

    bool load_from_file();
    bool save_to_file() const;
};

}  // namespace db::catalog
