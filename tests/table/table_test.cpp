#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "table/table.h"

namespace {

db::table::Row make_user_row(std::uint32_t key, const std::string& name, bool active) {
    return db::table::Row{
        key,
        {
            std::int64_t{key},
            name,
            active
        }
    };
}

void assert_user_row(
    const db::table::Row& row,
    std::uint32_t expected_key,
    const std::string& expected_name,
    bool expected_active
) {
    assert(row.key == expected_key);
    assert(row.values.size() == 3);
    assert(std::get<std::int64_t>(row.values[0]) == expected_key);
    assert(std::get<std::string>(row.values[1]) == expected_name);
    assert(std::get<bool>(row.values[2]) == expected_active);
}

const db::table::Row& require_row_by_key(
    const std::vector<db::table::Row>& rows,
    std::uint32_t key
) {
    for (const auto& row : rows) {
        if (row.key == key) {
            return row;
        }
    }

    assert(false);
    return rows[0];
}

}  // namespace

int main() {
    const std::string heap_file_name = "table_test_heap.db";
    const std::string index_file_name = "table_test_index.db";
    const std::string schema_heap_file_name = "table_schema_test_heap.db";
    const std::string schema_index_file_name = "table_schema_test_index.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(index_file_name);
    std::filesystem::remove(schema_heap_file_name);
    std::filesystem::remove(schema_index_file_name);

    {
        db::table::Table table("users", heap_file_name, index_file_name, 8);

        auto missing_before_insert = table.get_by_key(42);
        assert(!missing_before_insert.has_value());

        auto row_id_1 = table.insert(make_user_row(10, "Alice", true));
        auto row_id_2 = table.insert(make_user_row(20, "Bob", false));
        auto row_id_3 = table.insert(make_user_row(30, "Carol", true));

        assert(row_id_1.has_value());
        assert(row_id_2.has_value());
        assert(row_id_3.has_value());

        auto found_10 = table.get_by_key(10);
        auto found_20 = table.get_by_key(20);
        auto found_30 = table.get_by_key(30);
        auto missing = table.get_by_key(999);

        assert(found_10.has_value());
        assert_user_row(found_10.value(), 10, "Alice", true);

        assert(found_20.has_value());
        assert_user_row(found_20.value(), 20, "Bob", false);

        assert(found_30.has_value());
        assert_user_row(found_30.value(), 30, "Carol", true);

        assert(!missing.has_value());

        auto scanned_rows = table.scan();
        assert(scanned_rows.size() == 3);
        assert_user_row(require_row_by_key(scanned_rows, 10), 10, "Alice", true);
        assert_user_row(require_row_by_key(scanned_rows, 20), 20, "Bob", false);
        assert_user_row(require_row_by_key(scanned_rows, 30), 30, "Carol", true);

        // Duplicate primary key should fail because the current B+ tree rejects duplicates.
        auto duplicate_insert = table.insert(make_user_row(20, "Duplicate", true));
        assert(!duplicate_insert.has_value());

        // Same-key updates should succeed and be visible through indexed lookup.
        auto update_result = table.update_by_key(20, make_user_row(20, "Bobby", true));
        assert(update_result);

        auto relocation_update = table.update_by_key(
            20,
            make_user_row(20, "this is a much longer string that should be more likely to move", false)
        );
        assert(relocation_update);

        found_20 = table.get_by_key(20);
        assert(found_20.has_value());
        assert_user_row(
            found_20.value(),
            20,
            "this is a much longer string that should be more likely to move",
            false
        );

        scanned_rows = table.scan();
        assert(scanned_rows.size() == 3);
        assert_user_row(require_row_by_key(scanned_rows, 20), 20,
            "this is a much longer string that should be more likely to move",
            false
        );

        // Changing the primary key is out of scope for the first update path.
        auto invalid_key_change = table.update_by_key(20, make_user_row(21, "Wrong key", true));
        assert(!invalid_key_change);
    }

    // Reopen the table to verify that both heap data and index root metadata persist.
    {
        db::table::Table reopened_table("users", heap_file_name, index_file_name, 8);

        auto found_10 = reopened_table.get_by_key(10);
        auto found_20 = reopened_table.get_by_key(20);
        auto found_30 = reopened_table.get_by_key(30);
        auto missing = reopened_table.get_by_key(999);

        assert(found_10.has_value());
        assert_user_row(found_10.value(), 10, "Alice", true);

        assert(found_20.has_value());
        assert_user_row(
            found_20.value(),
            20,
            "this is a much longer string that should be more likely to move",
            false
        );

        assert(found_30.has_value());
        assert_user_row(found_30.value(), 30, "Carol", true);

        assert(!missing.has_value());

        auto scanned_rows = reopened_table.scan();
        assert(scanned_rows.size() == 3);
        assert_user_row(require_row_by_key(scanned_rows, 10), 10, "Alice", true);
        assert_user_row(require_row_by_key(scanned_rows, 20), 20,
            "this is a much longer string that should be more likely to move",
            false
        );
        assert_user_row(require_row_by_key(scanned_rows, 30), 30, "Carol", true);
    }

    {
        db::table::TableMetadata metadata{
            "schema_users",
            schema_heap_file_name,
            schema_index_file_name,
            8,
            {
                {"id", db::table::ColumnType::Integer, true},
                {"name", db::table::ColumnType::String, false},
                {"active", db::table::ColumnType::Boolean, false}
            },
            {}
        };

        db::table::Table table(metadata);

        auto valid_insert = table.insert(make_user_row(100, "Dora", true));
        assert(valid_insert.has_value());

        auto found = table.get_by_key(100);
        assert(found.has_value());
        assert_user_row(found.value(), 100, "Dora", true);

        db::table::Row missing_field{
            101,
            {
                std::int64_t{101},
                std::string{"Missing Active"}
            }
        };
        assert(!table.insert(missing_field).has_value());

        db::table::Row wrong_type{
            102,
            {
                std::int64_t{102},
                std::int64_t{999},
                true
            }
        };
        assert(!table.insert(wrong_type).has_value());

        db::table::Row mismatched_primary_key{
            103,
            {
                std::int64_t{999},
                std::string{"Wrong Id"},
                true
            }
        };
        assert(!table.insert(mismatched_primary_key).has_value());

        assert(!table.update_by_key(100, wrong_type));
        assert(table.update_by_key(100, make_user_row(100, "Dora Updated", false)));

        found = table.get_by_key(100);
        assert(found.has_value());
        assert_user_row(found.value(), 100, "Dora Updated", false);

        auto scanned_rows = table.scan();
        assert(scanned_rows.size() == 1);
        assert_user_row(scanned_rows[0], 100, "Dora Updated", false);
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(index_file_name);
    std::filesystem::remove(schema_heap_file_name);
    std::filesystem::remove(schema_index_file_name);

    std::cout << "Table test passed.\n";
    return 0;
}
