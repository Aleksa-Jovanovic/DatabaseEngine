#include <cassert>
#include <filesystem>
#include <iostream>
#include <sstream>
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
    const std::string heap_file_name = "executor_delete_users_heap.db";
    const std::string primary_index_file_name = "executor_delete_users_primary_index.db";

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
        parser.parse("INSERT INTO users VALUES (3, 'Carol', FALSE);")
    ).success);

    {
        const db::execution::ExecutionResult delete_result =
            executor.execute(parser.parse("DELETE FROM users WHERE id = 1;"));

        assert(delete_result.success);
        assert(delete_result.error_message.empty());
        assert(delete_result.affected_rows == 1);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users;"));

        assert(select_result.success);
        assert(select_result.rows.size() == 2);
        assert_user_row(require_row_by_key(select_result.rows, 2), 2, "Bob", false);
        assert_user_row(require_row_by_key(select_result.rows, 3), 3, "Carol", false);

        const db::execution::ExecutionResult deleted_select_result =
            executor.execute(parser.parse("SELECT * FROM users WHERE id = 1;"));

        assert(deleted_select_result.success);
        assert(deleted_select_result.rows.empty());
    }

    {
        const db::execution::ExecutionResult delete_result =
            executor.execute(parser.parse("DELETE FROM users WHERE active = FALSE;"));

        assert(delete_result.success);
        assert(delete_result.error_message.empty());
        assert(delete_result.affected_rows == 2);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users;"));

        assert(select_result.success);
        assert(select_result.rows.empty());
    }

    assert(executor.execute(
        parser.parse("INSERT INTO users VALUES (4, 'Dora', TRUE);")
    ).success);
    assert(executor.execute(
        parser.parse("INSERT INTO users VALUES (5, 'Eve', TRUE);")
    ).success);

    {
        const db::execution::ExecutionResult delete_result =
            executor.execute(parser.parse("DELETE FROM users;"));

        assert(delete_result.success);
        assert(delete_result.error_message.empty());
        assert(delete_result.affected_rows == 2);

        const db::execution::ExecutionResult select_result =
            executor.execute(parser.parse("SELECT * FROM users;"));

        assert(select_result.success);
        assert(select_result.rows.empty());
    }

    {
        const db::execution::ExecutionResult delete_result =
            executor.execute(parser.parse("DELETE FROM users WHERE id = 999;"));

        assert(delete_result.success);
        assert(delete_result.error_message.empty());
        assert(delete_result.affected_rows == 0);
    }

    {
        const db::execution::ExecutionResult delete_result =
            executor.execute(parser.parse("DELETE FROM missing_users WHERE id = 1;"));

        assert(!delete_result.success);
        assert(!delete_result.error_message.empty());
        assert(delete_result.affected_rows == 0);
    }

    {
        const std::string bulk_heap_file_name = "executor_delete_bulk_users_heap.db";
        const std::string bulk_primary_index_file_name =
            "executor_delete_bulk_users_primary_index.db";

        std::filesystem::remove(bulk_heap_file_name);
        std::filesystem::remove(bulk_primary_index_file_name);

        db::catalog::TableDefinition bulk_users_table{
            "bulk_users",
            user_schema,
            bulk_heap_file_name,
            {
                {
                    "bulk_users_pkey",
                    "bulk_users",
                    "id",
                    bulk_primary_index_file_name,
                    true,
                    true
                }
            }
        };

        db::catalog::Catalog bulk_catalog;
        assert(bulk_catalog.create_table(bulk_users_table));

        db::execution::Executor bulk_executor(bulk_catalog);

        constexpr std::uint32_t row_count = 10000;
        for (std::uint32_t id = 1; id <= row_count; ++id) {
            std::ostringstream insert_sql;
            insert_sql << "INSERT INTO bulk_users VALUES ("
                       << id
                       << ", 'User "
                       << id
                       << "', TRUE);";

            assert(bulk_executor.execute(parser.parse(insert_sql.str())).success);
        }

        const db::execution::ExecutionResult delete_result =
            bulk_executor.execute(parser.parse("DELETE FROM bulk_users;"));

        assert(delete_result.success);
        assert(delete_result.error_message.empty());
        assert(delete_result.affected_rows == row_count);

        const db::execution::ExecutionResult select_result =
            bulk_executor.execute(parser.parse("SELECT * FROM bulk_users;"));

        assert(select_result.success);
        assert(select_result.rows.empty());

        std::filesystem::remove(bulk_heap_file_name);
        std::filesystem::remove(bulk_primary_index_file_name);
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    std::cout << "Executor delete test passed.\n";
    return 0;
}
