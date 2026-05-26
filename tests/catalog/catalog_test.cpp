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

    assert(!catalog.create_table(users_table));

    db::catalog::TableDefinition invalid_table{
        "",
        db::catalog::Schema{},
        "invalid_heap.db",
        {}
    };
    assert(!catalog.create_table(invalid_table));

    const db::catalog::CatalogMetadata& metadata = catalog.metadata();
    assert(metadata.tables.size() == 1);
    assert(metadata.tables[0].table_name == "users");

    std::cout << "Catalog test passed.\n";
    return 0;
}
