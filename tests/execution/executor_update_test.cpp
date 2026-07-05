#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

#include "catalog/catalog.h"
#include "execution/executor.h"
#include "sql/parser.h"
#include "table/row.h"

namespace {

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
    const std::string& expected_name,
    bool expected_active
) {
    assert(row.key == expected_key);
    assert(row.values.size() == 3);
    assert(std::get<std::int64_t>(row.values[0]) == expected_key);
    assert(std::get<std::string>(row.values[1]) == expected_name);
    assert(std::get<bool>(row.values[2]) == expected_active);
}

}  // namespace

int main() {
    const std::string heap_file_name = "executor_update_users_heap.db";
    const std::string primary_index_file_name = "executor_update_users_primary_index.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    db::catalog::Schema user_schema;
    user_schema.add_column({"id", db::catalog::ColumnType::Integer, true});
    user_schema.add_column({"name", db::catalog::ColumnType::String, false});
    user_schema.add_column({"active", db::catalog::ColumnType::Boolean, false});

    db::catalog::TableDefinition users_table{
        "users",
        user_schema,
        heap_file_name,
        {
            {
                "users_pkey",
                "users",
                "id",
                primary_index_file_name,
                true,
                true
            }
        }
    };

    db::catalog::Catalog catalog;
    assert(catalog.create_table(users_table));

    db::sql::Parser parser;
    db::execution::Executor executor(catalog);

    assert(executor.execute(
        parser.parse("INSERT INTO users VALUES (1, 'Alice', TRUE);")
    ).success);
    assert(executor.execute(
        parser.parse("INSERT INTO users VALUES (2, 'Bob', FALSE);")
    ).success);
    assert(executor.execute(
        parser.parse("INSERT INTO users VALUES (3, 'Carol', TRUE);")
    ).success);

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET name = 'Alice Updated' WHERE id = 1;"));

        assert(update_result.success);
        assert(update_result.error_message.empty());
        assert(update_result.affected_rows == 1);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users WHERE id = 1;"));

        assert(select_result.success);
        assert(select_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(select_result.rows, 1),
            1,
            "Alice Updated",
            true
        );
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET active = TRUE WHERE name = 'Bob';"));

        assert(update_result.success);
        assert(update_result.error_message.empty());
        assert(update_result.affected_rows == 1);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users WHERE id = 2;"));

        assert(select_result.success);
        assert(select_result.rows.size() == 1);
        assert_user_row(
            require_row_by_key(select_result.rows, 2),
            2,
            "Bob",
            true
        );
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET active = FALSE;"));

        assert(update_result.success);
        assert(update_result.error_message.empty());
        assert(update_result.affected_rows == 3);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users;"));

        assert(select_result.success);
        assert(select_result.rows.size() == 3);
        assert_user_row(
            require_row_by_key(select_result.rows, 1),
            1,
            "Alice Updated",
            false
        );
        assert_user_row(require_row_by_key(select_result.rows, 2), 2, "Bob", false);
        assert_user_row(require_row_by_key(select_result.rows, 3), 3, "Carol", false);
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET name = 'Nobody' WHERE id = 999;"));

        assert(update_result.success);
        assert(update_result.error_message.empty());
        assert(update_result.affected_rows == 0);
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET id = 10 WHERE id = 1;"));

        assert(!update_result.success);
        assert(!update_result.error_message.empty());
        assert(update_result.affected_rows == 0);
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE missing_users SET name = 'Ghost' WHERE id = 1;"));

        assert(!update_result.success);
        assert(!update_result.error_message.empty());
        assert(update_result.affected_rows == 0);
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET missing_column = 'Bad' WHERE id = 1;"));

        assert(!update_result.success);
        assert(!update_result.error_message.empty());
        assert(update_result.affected_rows == 0);
    }

    {
        const db::execution::ExecutionResult update_result =
            executor.execute(parser.parse("UPDATE users SET active = 'not boolean' WHERE id = 1;"));

        assert(!update_result.success);
        assert(!update_result.error_message.empty());
        assert(update_result.affected_rows == 0);
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    std::cout << "Executor update test passed.\n";
    return 0;
}
