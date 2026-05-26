#include <cassert>
#include <filesystem>
#include <iostream>

#include "table/table.h"

int main() {
    const std::string heap_file_name = "table_test_heap.db";
    const std::string index_file_name = "table_test_index.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(index_file_name);

    {
        db::table::Table table("users", heap_file_name, index_file_name, 8);

        auto missing_before_insert = table.get_by_key(42);
        assert(!missing_before_insert.has_value());

        auto row_id_1 = table.insert(db::table::Row{10, "100"});
        auto row_id_2 = table.insert(db::table::Row{20, "200"});
        auto row_id_3 = table.insert(db::table::Row{30, "300"});

        assert(row_id_1.has_value());
        assert(row_id_2.has_value());
        assert(row_id_3.has_value());

        auto found_10 = table.get_by_key(10);
        auto found_20 = table.get_by_key(20);
        auto found_30 = table.get_by_key(30);
        auto missing = table.get_by_key(999);

        assert(found_10.has_value());
        assert(found_10->key == 10);
        assert(found_10->value == "100");

        assert(found_20.has_value());
        assert(found_20->key == 20);
        assert(found_20->value == "200");

        assert(found_30.has_value());
        assert(found_30->key == 30);
        assert(found_30->value == "300");

        assert(!missing.has_value());

        // Duplicate primary key should fail because the current B+ tree rejects duplicates.
        auto duplicate_insert = table.insert(db::table::Row{20, "999"});
        assert(!duplicate_insert.has_value());

        // Same-key updates should succeed and be visible through indexed lookup.
        auto update_result = table.update_by_key(20, db::table::Row{20, "999"});
        assert(update_result);

        auto relocation_update = table.update_by_key(
            20,
            db::table::Row{20, "this is a much longer string that should be more likely to move"}
        );
        assert(relocation_update);

        found_20 = table.get_by_key(20);
        assert(found_20.has_value());
        assert(found_20->key == 20);
        assert(found_20->value == "this is a much longer string that should be more likely to move");

        // Changing the primary key is out of scope for the first update path.
        auto invalid_key_change = table.update_by_key(20, db::table::Row{21, "111"});
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
        assert(found_10->key == 10);
        assert(found_10->value == "100");

        assert(found_20.has_value());
        assert(found_20->key == 20);
        assert(found_20->value == "this is a much longer string that should be more likely to move");

        assert(found_30.has_value());
        assert(found_30->key == 30);
        assert(found_30->value == "300");

        assert(!missing.has_value());
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(index_file_name);

    std::cout << "Table test passed.\n";
    return 0;
}