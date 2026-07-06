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
    const std::string heap_file_name = "executor_insert_users_heap.db";
    const std::string primary_index_file_name = "executor_insert_users_primary_index.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    db::catalog::Schema user_schema;
    user_schema.add_column({"id", db::catalog::ColumnType::Integer, true, true});
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

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users VALUES (1, 'Alice', TRUE);");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users (name, active, id) VALUES ('Bob', FALSE, 2);");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users (id, name) VALUES (3, 'Carol');");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users (name, active) VALUES ('Dora', TRUE);");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users VALUES ('Eve', FALSE);");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users (id, name) VALUES (10, 'Manual High Key');");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    {
        const db::sql::Statement insert_statement =
            parser.parse("INSERT INTO users (name) VALUES ('After Manual High Key');");
        const db::execution::ExecutionResult insert_result =
            executor.execute(insert_statement);

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
    }

    const db::sql::Statement select_statement =
        parser.parse("SELECT * FROM users;");
    const db::execution::ExecutionResult select_result =
        executor.execute(select_statement);

    assert(select_result.success);
    assert(select_result.error_message.empty());
    assert(select_result.rows.size() == 7);

    assert_user_row(require_row_by_key(select_result.rows, 1), 1, "Alice", true);
    assert_user_row(require_row_by_key(select_result.rows, 2), 2, "Bob", false);
    assert_user_row(require_row_by_key(select_result.rows, 3), 3, "Carol", false);
    assert_user_row(require_row_by_key(select_result.rows, 4), 4, "Dora", true);
    assert_user_row(require_row_by_key(select_result.rows, 5), 5, "Eve", false);
    assert_user_row(require_row_by_key(select_result.rows, 10), 10, "Manual High Key", false);
    assert_user_row(require_row_by_key(select_result.rows, 11), 11, "After Manual High Key", false);

    {
        const db::sql::Statement duplicate_insert_statement =
            parser.parse("INSERT INTO users VALUES (1, 'Duplicate Alice', FALSE);");
        const db::execution::ExecutionResult duplicate_insert_result =
            executor.execute(duplicate_insert_statement);

        assert(!duplicate_insert_result.success);
        assert(!duplicate_insert_result.error_message.empty());
    }

    {
        const db::sql::Statement missing_table_insert_statement =
            parser.parse("INSERT INTO missing_users VALUES (1, 'Nobody', FALSE);");
        const db::execution::ExecutionResult missing_table_insert_result =
            executor.execute(missing_table_insert_statement);

        assert(!missing_table_insert_result.success);
        assert(!missing_table_insert_result.error_message.empty());
    }

    {
        const db::sql::Statement invalid_type_insert_statement =
            parser.parse("INSERT INTO users VALUES ('wrong id', 'Bad', FALSE);");
        const db::execution::ExecutionResult invalid_type_insert_result =
            executor.execute(invalid_type_insert_statement);

        assert(!invalid_type_insert_result.success);
        assert(!invalid_type_insert_result.error_message.empty());
    }

    {
        const db::sql::Statement duplicate_column_insert_statement =
            parser.parse("INSERT INTO users (id, id, active) VALUES (20, 21, TRUE);");
        const db::execution::ExecutionResult duplicate_column_insert_result =
            executor.execute(duplicate_column_insert_statement);

        assert(!duplicate_column_insert_result.success);
        assert(!duplicate_column_insert_result.error_message.empty());
    }

    {
        const std::string citizens_heap_file_name = "executor_insert_citizens_heap.db";
        const std::string citizens_primary_index_file_name =
            "executor_insert_citizens_primary_index.db";

        std::filesystem::remove(citizens_heap_file_name);
        std::filesystem::remove(citizens_primary_index_file_name);

        db::catalog::Schema citizen_schema;
        citizen_schema.add_column({"jmbg", db::catalog::ColumnType::Integer, true, false});
        citizen_schema.add_column({"name", db::catalog::ColumnType::String, false, false});

        db::catalog::TableDefinition citizens_table{
            "citizens",
            citizen_schema,
            citizens_heap_file_name,
            {
                {
                    "citizens_pkey",
                    "citizens",
                    "jmbg",
                    citizens_primary_index_file_name,
                    true,
                    true
                }
            }
        };

        assert(catalog.create_table(citizens_table));

        const db::sql::Statement missing_primary_key_statement =
            parser.parse("INSERT INTO citizens (name) VALUES ('Manual Primary Key Required');");
        const db::execution::ExecutionResult missing_primary_key_result =
            executor.execute(missing_primary_key_statement);

        assert(!missing_primary_key_result.success);
        assert(!missing_primary_key_result.error_message.empty());

        const db::sql::Statement manual_primary_key_statement =
            parser.parse("INSERT INTO citizens VALUES (123456789, 'Aleksa');");
        const db::execution::ExecutionResult manual_primary_key_result =
            executor.execute(manual_primary_key_statement);

        assert(manual_primary_key_result.success);
        assert(manual_primary_key_result.error_message.empty());
        assert(manual_primary_key_result.affected_rows == 1);

        std::filesystem::remove(citizens_heap_file_name);
        std::filesystem::remove(citizens_primary_index_file_name);
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    std::cout << "Executor insert test passed.\n";
    return 0;
}
