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

void assert_projected_id_name_row(
    const db::table::Row& row,
    std::uint32_t expected_key,
    const std::string& expected_name
) {
    assert(row.key == expected_key);
    assert(row.values.size() == 2);
    assert(std::get<std::int64_t>(row.values[0]) == expected_key);
    assert(std::get<std::string>(row.values[1]) == expected_name);
}

}  // namespace

int main() {
    const std::string heap_file_name = "executor_users_heap.db";
    const std::string primary_index_file_name = "executor_users_primary_index.db";

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

    {
        auto table = catalog.open_table("users");
        assert(table != nullptr);

        assert(table->insert(make_user_row(1, "Alice", true)).has_value());
        assert(table->insert(make_user_row(2, "Bob", false)).has_value());
    }

    db::sql::Parser parser;
    const db::sql::Statement statement = parser.parse("SELECT * FROM users;");

    db::execution::Executor executor(catalog);
    const db::execution::ExecutionResult result = executor.execute(statement);

    assert(result.success);
    assert(result.error_message.empty());

    assert(result.column_names.size() == 3);
    assert(result.column_names[0] == "id");
    assert(result.column_names[1] == "name");
    assert(result.column_names[2] == "active");

    assert(result.rows.size() == 2);
    assert_user_row(require_row_by_key(result.rows, 1), 1, "Alice", true);
    assert_user_row(require_row_by_key(result.rows, 2), 2, "Bob", false);

    const db::sql::Statement projection_statement =
        parser.parse("SELECT id, name FROM users;");
    const db::execution::ExecutionResult projection_result =
        executor.execute(projection_statement);

    assert(projection_result.success);
    assert(projection_result.error_message.empty());

    assert(projection_result.column_names.size() == 2);
    assert(projection_result.column_names[0] == "id");
    assert(projection_result.column_names[1] == "name");

    assert(projection_result.rows.size() == 2);
    assert_projected_id_name_row(
        require_row_by_key(projection_result.rows, 1),
        1,
        "Alice"
    );
    assert_projected_id_name_row(
        require_row_by_key(projection_result.rows, 2),
        2,
        "Bob"
    );

    const db::sql::Statement where_id_statement =
        parser.parse("SELECT * FROM users WHERE id = 1;");
    const db::execution::ExecutionResult where_id_result =
        executor.execute(where_id_statement);

    assert(where_id_result.success);
    assert(where_id_result.error_message.empty());
    assert(where_id_result.column_names.size() == 3);
    assert(where_id_result.rows.size() == 1);
    assert_user_row(require_row_by_key(where_id_result.rows, 1), 1, "Alice", true);

    const db::sql::Statement where_greater_statement =
        parser.parse("SELECT * FROM users WHERE id > 1;");
    const db::execution::ExecutionResult where_greater_result =
        executor.execute(where_greater_statement);

    assert(where_greater_result.success);
    assert(where_greater_result.error_message.empty());
    assert(where_greater_result.rows.size() == 1);
    assert_user_row(require_row_by_key(where_greater_result.rows, 2), 2, "Bob", false);

    const db::sql::Statement where_boolean_statement =
        parser.parse("SELECT * FROM users WHERE active = TRUE;");
    const db::execution::ExecutionResult where_boolean_result =
        executor.execute(where_boolean_statement);

    assert(where_boolean_result.success);
    assert(where_boolean_result.error_message.empty());
    assert(where_boolean_result.rows.size() == 1);
    assert_user_row(require_row_by_key(where_boolean_result.rows, 1), 1, "Alice", true);

    const db::sql::Statement projected_where_statement =
        parser.parse("SELECT id FROM users WHERE name = 'Alice';");
    const db::execution::ExecutionResult projected_where_result =
        executor.execute(projected_where_statement);

    assert(projected_where_result.success);
    assert(projected_where_result.error_message.empty());
    assert(projected_where_result.column_names.size() == 1);
    assert(projected_where_result.column_names[0] == "id");
    assert(projected_where_result.rows.size() == 1);

    const db::table::Row& projected_where_row =
        require_row_by_key(projected_where_result.rows, 1);
    assert(projected_where_row.key == 1);
    assert(projected_where_row.values.size() == 1);
    assert(std::get<std::int64_t>(projected_where_row.values[0]) == 1);

    const db::sql::Statement between_statement =
        parser.parse("SELECT * FROM users WHERE id BETWEEN 1 AND 2;");
    const db::execution::ExecutionResult between_result =
        executor.execute(between_statement);

    assert(between_result.success);
    assert(between_result.error_message.empty());
    assert(between_result.rows.size() == 2);
    assert_user_row(require_row_by_key(between_result.rows, 1), 1, "Alice", true);
    assert_user_row(require_row_by_key(between_result.rows, 2), 2, "Bob", false);

    const db::sql::Statement and_statement =
        parser.parse("SELECT * FROM users WHERE active = TRUE AND id > 1;");
    const db::execution::ExecutionResult and_result =
        executor.execute(and_statement);

    assert(and_result.success);
    assert(and_result.error_message.empty());
    assert(and_result.rows.empty());

    const db::sql::Statement or_statement =
        parser.parse("SELECT * FROM users WHERE active = TRUE OR id > 1;");
    const db::execution::ExecutionResult or_result =
        executor.execute(or_statement);

    assert(or_result.success);
    assert(or_result.error_message.empty());
    assert(or_result.rows.size() == 2);
    assert_user_row(require_row_by_key(or_result.rows, 1), 1, "Alice", true);
    assert_user_row(require_row_by_key(or_result.rows, 2), 2, "Bob", false);

    const db::sql::Statement precedence_statement =
        parser.parse("SELECT * FROM users WHERE active = TRUE OR name = 'Bob' AND id = 999;");
    const db::execution::ExecutionResult precedence_result =
        executor.execute(precedence_statement);

    assert(precedence_result.success);
    assert(precedence_result.error_message.empty());
    assert(precedence_result.rows.size() == 1);
    assert_user_row(require_row_by_key(precedence_result.rows, 1), 1, "Alice", true);

    const db::sql::Statement missing_table_statement =
        parser.parse("SELECT * FROM missing_users;");
    const db::execution::ExecutionResult missing_table_result =
        executor.execute(missing_table_statement);

    assert(!missing_table_result.success);
    assert(!missing_table_result.error_message.empty());

    const db::sql::Statement missing_column_statement =
        parser.parse("SELECT missing_column FROM users;");
    const db::execution::ExecutionResult missing_column_result =
        executor.execute(missing_column_statement);

    assert(!missing_column_result.success);
    assert(!missing_column_result.error_message.empty());

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    std::cout << "Executor test passed.\n";
    return 0;
}
