#include <cassert>
#include <filesystem>
#include <iostream>

#include "catalog/catalog.h"

int main() {
    const std::string catalog_file_name = "catalog_test.db";
    std::filesystem::remove(catalog_file_name);

    db::catalog::Catalog catalog;

    db::catalog::Schema user_schema;
    user_schema.add_column({"id", db::catalog::ColumnType::Integer, true, true});
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
    assert(found_users->schema.columns()[0].is_auto_increment);

    assert(found_users->schema.columns()[1].name == "name");
    assert(found_users->schema.columns()[1].type == db::catalog::ColumnType::String);
    assert(!found_users->schema.columns()[1].is_primary_key);
    assert(!found_users->schema.columns()[1].is_auto_increment);

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

    assert(built_metadata->columns.size() == 3);
    assert(built_metadata->columns[0].name == "id");
    assert(built_metadata->columns[0].type == db::table::ColumnType::Integer);
    assert(built_metadata->columns[0].is_primary_key);
    assert(built_metadata->columns[0].is_auto_increment);

    assert(built_metadata->columns[1].name == "name");
    assert(built_metadata->columns[1].type == db::table::ColumnType::String);
    assert(!built_metadata->columns[1].is_primary_key);
    assert(!built_metadata->columns[1].is_auto_increment);

    assert(built_metadata->columns[2].name == "is_active");
    assert(built_metadata->columns[2].type == db::table::ColumnType::Boolean);
    assert(!built_metadata->columns[2].is_primary_key);
    assert(!built_metadata->columns[2].is_auto_increment);

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
        db::catalog::Schema{
            {
                {"id", db::catalog::ColumnType::Integer, true}
            }
        },
        "events_heap.db",
        {
            {
                "events_name_idx",
                "events",
                "id",
                "events_name_index.db",
                false,
                true
            }
        }
    };

    assert(!missing_primary_catalog.create_table(missing_primary_table));
    assert(!missing_primary_catalog.build_table_metadata("events").has_value());

    const db::catalog::CatalogMetadata& metadata = catalog.metadata();
    assert(metadata.tables.size() == 1);
    assert(metadata.tables[0].table_name == "users");

    const auto opened_table = catalog.open_table("users", 32);
    assert(opened_table != nullptr);

    const auto missing_open = catalog.open_table("missing_table");
    assert(missing_open == nullptr);

    // Drop should remove the table definition and its physical heap/index files.
    {
        const std::string drop_heap_file_name = "drop_catalog_users_heap.db";
        const std::string drop_primary_index_file_name =
            "drop_catalog_users_primary_index.db";
        const std::string drop_secondary_index_file_name =
            "drop_catalog_users_name_index.db";

        std::filesystem::remove(drop_heap_file_name);
        std::filesystem::remove(drop_primary_index_file_name);
        std::filesystem::remove(drop_secondary_index_file_name);

        db::catalog::Catalog drop_catalog;
        db::catalog::Schema drop_schema;
        drop_schema.add_column({"id", db::catalog::ColumnType::Integer, true, true});
        drop_schema.add_column({"name", db::catalog::ColumnType::String, false});

        db::catalog::TableDefinition drop_table{
            "drop_catalog_users",
            drop_schema,
            drop_heap_file_name,
            {
                {
                    "drop_catalog_users_pkey",
                    "drop_catalog_users",
                    "id",
                    drop_primary_index_file_name,
                    true,
                    true
                },
                {
                    "drop_catalog_users_name_idx",
                    "drop_catalog_users",
                    "name",
                    drop_secondary_index_file_name,
                    false,
                    true
                }
            }
        };

        assert(drop_catalog.create_table(drop_table));
        assert(drop_catalog.has_table("drop_catalog_users"));
        assert(std::filesystem::exists(drop_heap_file_name));
        assert(std::filesystem::exists(drop_primary_index_file_name));
        assert(std::filesystem::exists(drop_secondary_index_file_name));

        assert(drop_catalog.drop_table("drop_catalog_users"));
        assert(!drop_catalog.has_table("drop_catalog_users"));
        assert(!drop_catalog.find_table_definition("drop_catalog_users").has_value());
        assert(!std::filesystem::exists(drop_heap_file_name));
        assert(!std::filesystem::exists(drop_primary_index_file_name));
        assert(!std::filesystem::exists(drop_secondary_index_file_name));

        assert(!drop_catalog.drop_table("drop_catalog_users"));
    }

    {
        db::catalog::Catalog persistent_catalog(catalog_file_name);
        assert(persistent_catalog.create_table(users_table));
        assert(persistent_catalog.has_table("users"));
    }

    {
        db::catalog::Catalog reopened_catalog(catalog_file_name);
        assert(reopened_catalog.has_table("users"));

        const auto reopened_definition = reopened_catalog.find_table_definition("users");
        assert(reopened_definition.has_value());
        assert(reopened_definition->table_name == "users");
        assert(reopened_definition->heap_file_name == "users_heap.db");
        assert(reopened_definition->schema.columns().size() == 3);
        assert(reopened_definition->schema.columns()[0].is_primary_key);
        assert(reopened_definition->schema.columns()[0].is_auto_increment);
        assert(!reopened_definition->schema.columns()[1].is_auto_increment);
        assert(reopened_definition->indexes.size() == 2);

        const auto reopened_table = reopened_catalog.open_table("users", 24);
        assert(reopened_table != nullptr);
    }

    // Validation: duplicate column names should be rejected.
    {
        db::catalog::Catalog duplicate_column_catalog;
        db::catalog::Schema duplicate_column_schema;
        duplicate_column_schema.add_column({"id", db::catalog::ColumnType::Integer, true});
        duplicate_column_schema.add_column({"id", db::catalog::ColumnType::String, false});

        db::catalog::TableDefinition duplicate_column_table{
            "duplicate_columns",
            duplicate_column_schema,
            "duplicate_columns_heap.db",
            {
                {
                    "duplicate_columns_pkey",
                    "duplicate_columns",
                    "id",
                    "duplicate_columns_primary_index.db",
                    true,
                    true
                }
            }
        };

        assert(!duplicate_column_catalog.create_table(duplicate_column_table));
    }

    // Validation: duplicate index names should be rejected.
    {
        db::catalog::Catalog duplicate_index_catalog;
        db::catalog::Schema duplicate_index_schema;
        duplicate_index_schema.add_column({"id", db::catalog::ColumnType::Integer, true});
        duplicate_index_schema.add_column({"name", db::catalog::ColumnType::String, false});

        db::catalog::TableDefinition duplicate_index_table{
            "duplicate_indexes",
            duplicate_index_schema,
            "duplicate_indexes_heap.db",
            {
                {
                    "dup_idx",
                    "duplicate_indexes",
                    "id",
                    "duplicate_indexes_primary_index.db",
                    true,
                    true
                },
                {
                    "dup_idx",
                    "duplicate_indexes",
                    "name",
                    "duplicate_indexes_name_index.db",
                    false,
                    true
                }
            }
        };

        assert(!duplicate_index_catalog.create_table(duplicate_index_table));
    }

    // Validation: non-integer primary key should be rejected.
    {
        db::catalog::Catalog string_primary_catalog;
        db::catalog::Schema string_primary_schema;
        string_primary_schema.add_column({"email", db::catalog::ColumnType::String, true});

        db::catalog::TableDefinition string_primary_table{
            "accounts",
            string_primary_schema,
            "accounts_heap.db",
            {
                {
                    "accounts_pkey",
                    "accounts",
                    "email",
                    "accounts_primary_index.db",
                    true,
                    true
                }
            }
        };

        assert(!string_primary_catalog.create_table(string_primary_table));
    }

    // Validation: primary index column must match the schema primary-key column.
    {
        db::catalog::Catalog mismatched_primary_catalog;
        db::catalog::Schema mismatched_primary_schema;
        mismatched_primary_schema.add_column({"id", db::catalog::ColumnType::Integer, true});
        mismatched_primary_schema.add_column({"name", db::catalog::ColumnType::String, false});

        db::catalog::TableDefinition mismatched_primary_table{
            "mismatched_primary",
            mismatched_primary_schema,
            "mismatched_primary_heap.db",
            {
                {
                    "mismatched_primary_pkey",
                    "mismatched_primary",
                    "name",
                    "mismatched_primary_index.db",
                    true,
                    true
                }
            }
        };

        assert(!mismatched_primary_catalog.create_table(mismatched_primary_table));
    }

    // Validation: auto-increment is only valid on the primary-key column.
    {
        db::catalog::Catalog invalid_auto_increment_catalog;
        db::catalog::Schema invalid_auto_increment_schema;
        invalid_auto_increment_schema.add_column({"id", db::catalog::ColumnType::Integer, true, false});
        invalid_auto_increment_schema.add_column({"serial", db::catalog::ColumnType::Integer, false, true});

        db::catalog::TableDefinition invalid_auto_increment_table{
            "invalid_auto_increment",
            invalid_auto_increment_schema,
            "invalid_auto_increment_heap.db",
            {
                {
                    "invalid_auto_increment_pkey",
                    "invalid_auto_increment",
                    "id",
                    "invalid_auto_increment_primary_index.db",
                    true,
                    true
                }
            }
        };

        assert(!invalid_auto_increment_catalog.create_table(invalid_auto_increment_table));
    }

    std::filesystem::remove(catalog_file_name);

    std::cout << "Catalog test passed.\n";
    return 0;
}
