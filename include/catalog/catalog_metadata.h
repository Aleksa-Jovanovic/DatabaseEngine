#pragma once

#include <string>
#include <vector>

#include "catalog/schema.h"

namespace db::catalog {

struct IndexDefinition {
    std::string index_name;
    std::string table_name;
    std::string column_name;
    std::string file_name;
    bool is_primary = false;
    bool is_unique = true;
};

struct TableDefinition {
    std::string table_name;
    Schema schema;
    std::string heap_file_name;
    std::vector<IndexDefinition> indexes;
};

// CatalogMetadata is the database-wide metadata container. Later it can become
// the persistent source of truth for table and index definitions, from which
// table-local runtime metadata objects are derived.
struct CatalogMetadata {
    std::vector<TableDefinition> tables;
};

}  // namespace db::catalog
