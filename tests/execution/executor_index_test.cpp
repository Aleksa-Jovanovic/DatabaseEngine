#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

#include "catalog/catalog.h"
#include "execution/executor.h"
#include "index/bplustree.h"
#include "index/index_key.h"
#include "sql/parser.h"

namespace {

db::index::IndexKey require_secondary_key(
    std::int64_t indexed_value,
    std::uint32_t primary_key
) {
    const auto encoded_key =
        db::index::encode_secondary_integer_key(indexed_value, primary_key);
    assert(encoded_key.has_value());
    return encoded_key.value();
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

void assert_user_row(
    const db::table::Row& row,
    std::uint32_t expected_key,
    std::int64_t expected_age,
    const std::string& expected_name
) {
    assert(row.key == expected_key);
    assert(row.values.size() == 3);
    assert(std::get<std::int64_t>(row.values[0]) == expected_key);
    assert(std::get<std::int64_t>(row.values[1]) == expected_age);
    assert(std::get<std::string>(row.values[2]) == expected_name);
}

}  // namespace

int main() {
    const std::string heap_file_name = "users_heap.db";
    const std::string primary_index_file_name = "users_primary_index.db";
    const std::string age_index_file_name =
        "users_users_age_idx.db";
    const std::string name_index_file_name =
        "users_users_name_idx.db";
    const std::string primary_duplicate_index_file_name =
        "users_users_id_idx.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);
    std::filesystem::remove(age_index_file_name);
    std::filesystem::remove(name_index_file_name);
    std::filesystem::remove(primary_duplicate_index_file_name);

    db::catalog::Catalog catalog;
    db::sql::Parser parser;
    db::execution::Executor executor(catalog);

    {
        const db::execution::ExecutionResult create_table_result = executor.execute(
            parser.parse("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, age INTEGER, name STRING);")
        );

        assert(create_table_result.success);
        assert(create_table_result.error_message.empty());
        assert(std::filesystem::exists(heap_file_name));
        assert(std::filesystem::exists(primary_index_file_name));
    }

    {
        const db::execution::ExecutionResult insert_alice_result = executor.execute(
            parser.parse("INSERT INTO users (age, name) VALUES (30, 'Alice');")
        );
        const db::execution::ExecutionResult insert_bob_result = executor.execute(
            parser.parse("INSERT INTO users (age, name) VALUES (30, 'Bob');")
        );
        const db::execution::ExecutionResult insert_carol_result = executor.execute(
            parser.parse("INSERT INTO users (age, name) VALUES (40, 'Carol');")
        );

        assert(insert_alice_result.success);
        assert(insert_bob_result.success);
        assert(insert_carol_result.success);
    }

    {
        const db::execution::ExecutionResult create_index_result = executor.execute(
            parser.parse("CREATE INDEX users_age_idx ON users (age);")
        );

        assert(create_index_result.success);
        assert(create_index_result.error_message.empty());
        assert(create_index_result.affected_rows == 0);
        assert(std::filesystem::exists(age_index_file_name));

        const auto table_definition = catalog.find_table_definition("users");
        assert(table_definition.has_value());
        assert(table_definition->indexes.size() == 2);
        assert(table_definition->indexes[1].index_name == "users_age_idx");
        assert(table_definition->indexes[1].column_name == "age");
        assert(table_definition->indexes[1].file_name == age_index_file_name);
        assert(!table_definition->indexes[1].is_primary);
        assert(table_definition->indexes[1].is_unique);
    }

    {
        db::index::BPlusTree age_index(age_index_file_name, 8);

        assert(age_index.search(require_secondary_key(30, 1)).has_value());
        assert(age_index.search(require_secondary_key(30, 2)).has_value());
        assert(age_index.search(require_secondary_key(40, 3)).has_value());
    }

    {
        const db::execution::ExecutionResult primary_index_select_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE id = 2;")
        );

        assert(primary_index_select_result.success);
        assert(primary_index_select_result.error_message.empty());
        assert(primary_index_select_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(primary_index_select_result.rows, 2),
            2,
            30,
            "Bob"
        );
    }

    {
        const db::execution::ExecutionResult secondary_index_select_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE age = 30;")
        );

        assert(secondary_index_select_result.success);
        assert(secondary_index_select_result.error_message.empty());
        assert(secondary_index_select_result.rows.size() == 2);
        assert_user_row(
            require_row_by_key(secondary_index_select_result.rows, 1),
            1,
            30,
            "Alice"
        );
        assert_user_row(
            require_row_by_key(secondary_index_select_result.rows, 2),
            2,
            30,
            "Bob"
        );
    }

    {
        const db::execution::ExecutionResult and_primary_index_select_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE age = 30 AND id = 2;")
        );

        assert(and_primary_index_select_result.success);
        assert(and_primary_index_select_result.error_message.empty());
        assert(and_primary_index_select_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(and_primary_index_select_result.rows, 2),
            2,
            30,
            "Bob"
        );
    }

    {
        const db::execution::ExecutionResult and_secondary_index_select_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE name = 'Alice' AND age = 30;")
        );

        assert(and_secondary_index_select_result.success);
        assert(and_secondary_index_select_result.error_message.empty());
        assert(and_secondary_index_select_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(and_secondary_index_select_result.rows, 1),
            1,
            30,
            "Alice"
        );
    }

    {
        const db::execution::ExecutionResult or_fallback_select_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE age = 30 OR id = 3;")
        );

        assert(or_fallback_select_result.success);
        assert(or_fallback_select_result.error_message.empty());
        assert(or_fallback_select_result.rows.size() == 3);
        assert_user_row(
            require_row_by_key(or_fallback_select_result.rows, 1),
            1,
            30,
            "Alice"
        );
        assert_user_row(
            require_row_by_key(or_fallback_select_result.rows, 2),
            2,
            30,
            "Bob"
        );
        assert_user_row(
            require_row_by_key(or_fallback_select_result.rows, 3),
            3,
            40,
            "Carol"
        );
    }

    {
        const db::execution::ExecutionResult primary_greater_equal_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE id >= 2;")
        );

        assert(primary_greater_equal_result.success);
        assert(primary_greater_equal_result.error_message.empty());
        assert(primary_greater_equal_result.rows.size() == 2);
        assert_user_row(
            require_row_by_key(primary_greater_equal_result.rows, 2),
            2,
            30,
            "Bob"
        );
        assert_user_row(
            require_row_by_key(primary_greater_equal_result.rows, 3),
            3,
            40,
            "Carol"
        );
    }

    {
        const db::execution::ExecutionResult primary_less_than_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE id < 3;")
        );

        assert(primary_less_than_result.success);
        assert(primary_less_than_result.error_message.empty());
        assert(primary_less_than_result.rows.size() == 2);
        assert_user_row(
            require_row_by_key(primary_less_than_result.rows, 1),
            1,
            30,
            "Alice"
        );
        assert_user_row(
            require_row_by_key(primary_less_than_result.rows, 2),
            2,
            30,
            "Bob"
        );
    }

    {
        const db::execution::ExecutionResult secondary_greater_equal_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE age >= 40;")
        );

        assert(secondary_greater_equal_result.success);
        assert(secondary_greater_equal_result.error_message.empty());
        assert(secondary_greater_equal_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(secondary_greater_equal_result.rows, 3),
            3,
            40,
            "Carol"
        );
    }

    {
        const db::execution::ExecutionResult secondary_less_than_result = executor.execute(
            parser.parse("SELECT * FROM users WHERE age < 40;")
        );

        assert(secondary_less_than_result.success);
        assert(secondary_less_than_result.error_message.empty());
        assert(secondary_less_than_result.rows.size() == 2);
        assert_user_row(
            require_row_by_key(secondary_less_than_result.rows, 1),
            1,
            30,
            "Alice"
        );
        assert_user_row(
            require_row_by_key(secondary_less_than_result.rows, 2),
            2,
            30,
            "Bob"
        );
    }

    {
        const db::execution::ExecutionResult duplicate_index_result = executor.execute(
            parser.parse("CREATE INDEX users_age_idx ON users (age);")
        );

        assert(!duplicate_index_result.success);
        assert(!duplicate_index_result.error_message.empty());
    }

    {
        const db::execution::ExecutionResult string_index_result = executor.execute(
            parser.parse("CREATE INDEX users_name_idx ON users (name);")
        );

        assert(!string_index_result.success);
        assert(!string_index_result.error_message.empty());
        assert(!std::filesystem::exists(name_index_file_name));
    }

    {
        const db::execution::ExecutionResult primary_key_index_result = executor.execute(
            parser.parse("CREATE INDEX users_id_idx ON users (id);")
        );

        assert(!primary_key_index_result.success);
        assert(!primary_key_index_result.error_message.empty());
        assert(!std::filesystem::exists(primary_duplicate_index_file_name));
    }

    {
        const db::execution::ExecutionResult missing_table_index_result = executor.execute(
            parser.parse("CREATE INDEX missing_age_idx ON missing_users (age);")
        );

        assert(!missing_table_index_result.success);
        assert(!missing_table_index_result.error_message.empty());
    }

    {
        const db::execution::ExecutionResult drop_primary_index_result = executor.execute(
            parser.parse("DROP INDEX users_pkey;")
        );

        assert(!drop_primary_index_result.success);
        assert(!drop_primary_index_result.error_message.empty());
        assert(std::filesystem::exists(primary_index_file_name));
    }

    {
        const db::execution::ExecutionResult drop_index_result = executor.execute(
            parser.parse("DROP INDEX users_age_idx;")
        );

        assert(drop_index_result.success);
        assert(drop_index_result.error_message.empty());
        assert(drop_index_result.affected_rows == 0);
        assert(!std::filesystem::exists(age_index_file_name));

        const auto table_definition = catalog.find_table_definition("users");
        assert(table_definition.has_value());
        assert(table_definition->indexes.size() == 1);
        assert(table_definition->indexes[0].is_primary);
    }

    {
        const db::execution::ExecutionResult drop_missing_index_result = executor.execute(
            parser.parse("DROP INDEX users_age_idx;")
        );

        assert(!drop_missing_index_result.success);
        assert(!drop_missing_index_result.error_message.empty());
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);
    std::filesystem::remove(age_index_file_name);
    std::filesystem::remove(name_index_file_name);
    std::filesystem::remove(primary_duplicate_index_file_name);

    std::cout << "Executor index test passed.\n";
    return 0;
}
