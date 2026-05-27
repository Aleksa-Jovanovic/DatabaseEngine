#include <cassert>
#include <iostream>

#include "catalog/catalog.h"

int main() {
    db::catalog::Catalog catalog;

    db::catalog::Schema user_schema;
    user_schema.add_column({"id", db::catalog::ColumnType::Integer, true});
    user_schema.add_column({"name", db::catalog::ColumnType::String, false});
    user_schema.add_column({"is_active", db::catalog::ColumnType::Boolean, false});

    db::catalog::TableDefinition users_table{
        "users",
        user_schema,
        "users_heap.db",
        {
            {
                "users_pkey",
                "users",
                "id",
                "users_primary_index.db",
                true,
                true
            },
            {
                "users_name_idx",
                "users",
                "name",
                "users_name_index.db",
                false,
                true
            }
        }
    };

    assert(!catalog.has_table("users"));
    assert(!catalog.find_table_definition("users").has_value());

    assert(catalog.create_table(users_table));
    assert(catalog.has_table("users"));

    const auto found_users = catalog.find_table_definition("users");
    assert(found_users.has_value());
    assert(found_users->table_name == "users");
    assert(found_users->heap_file_name == "users_heap.db");

    assert(found_users->schema.columns().size() == 3);
    assert(found_users->schema.columns()[0].name == "id");
    assert(found_users->schema.columns()[0].type == db::catalog::ColumnType::Integer);
    assert(found_users->schema.columns()[0].is_primary_key);

    assert(found_users->schema.columns()[1].name == "name");
    assert(found_users->schema.columns()[1].type == db::catalog::ColumnType::String);
    assert(!found_users->schema.columns()[1].is_primary_key);

    assert(found_users->indexes.size() == 2);
    assert(found_users->indexes[0].index_name == "users_pkey");
    assert(found_users->indexes[0].table_name == "users");
    assert(found_users->indexes[0].column_name == "id");
    assert(found_users->indexes[0].file_name == "users_primary_index.db");
    assert(found_users->indexes[0].is_primary);
    assert(found_users->indexes[0].is_unique);

    assert(found_users->indexes[1].index_name == "users_name_idx");
    assert(found_users->indexes[1].table_name == "users");
    assert(found_users->indexes[1].column_name == "name");
    assert(found_users->indexes[1].file_name == "users_name_index.db");
    assert(!found_users->indexes[1].is_primary);
    assert(found_users->indexes[1].is_unique);

    const auto built_metadata = catalog.build_table_metadata("users", 16);
    assert(built_metadata.has_value());
    assert(built_metadata->table_name == "users");
    assert(built_metadata->heap_file_name == "users_heap.db");
    assert(built_metadata->primary_index_file_name == "users_primary_index.db");
    assert(built_metadata->cache_size == 16);

    assert(built_metadata->secondary_indexes.size() == 1);
    assert(built_metadata->secondary_indexes[0].index_name == "users_name_idx");
    assert(built_metadata->secondary_indexes[0].column_name == "name");
    assert(built_metadata->secondary_indexes[0].file_name == "users_name_index.db");
    assert(!built_metadata->secondary_indexes[0].is_primary);
    assert(built_metadata->secondary_indexes[0].is_unique);

    assert(!catalog.build_table_metadata("missing_table").has_value());

    assert(!catalog.create_table(users_table));

    db::catalog::TableDefinition invalid_table{
        "",
        db::catalog::Schema{},
        "invalid_heap.db",
        {}
    };
    assert(!catalog.create_table(invalid_table));

    db::catalog::Catalog missing_primary_catalog;
    db::catalog::TableDefinition missing_primary_table{
        "events",
        db::catalog::Schema{},
        "events_heap.db",
        {
            {
                "events_name_idx",
                "events",
                "name",
                "events_name_index.db",
                false,
                true
            }
        }
    };

    assert(missing_primary_catalog.create_table(missing_primary_table));
    assert(!missing_primary_catalog.build_table_metadata("events").has_value());

    const db::catalog::CatalogMetadata& metadata = catalog.metadata();
    assert(metadata.tables.size() == 1);
    assert(metadata.tables[0].table_name == "users");

    const auto opened_table = catalog.open_table("users", 32);
    assert(opened_table != nullptr);

    const auto missing_open = catalog.open_table("missing_table");
    assert(missing_open == nullptr);

    std::cout << "Catalog test passed.\n";
    return 0;
}