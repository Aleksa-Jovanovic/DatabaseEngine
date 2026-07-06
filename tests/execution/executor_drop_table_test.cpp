#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "catalog/catalog.h"
#include "execution/executor.h"
#include "sql/parser.h"

int main() {
    const std::string heap_file_name = "executor_drop_users_heap.db";
    const std::string primary_index_file_name =
        "executor_drop_users_primary_index.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    db::catalog::Catalog catalog;
    db::sql::Parser parser;
    db::execution::Executor executor(catalog);

    {
        const db::execution::ExecutionResult create_result = executor.execute(
            parser.parse("CREATE TABLE executor_drop_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name STRING);")
        );

        assert(create_result.success);
        assert(create_result.error_message.empty());
        assert(catalog.has_table("executor_drop_users"));
        assert(std::filesystem::exists(heap_file_name));
        assert(std::filesystem::exists(primary_index_file_name));
    }

    {
        const db::execution::ExecutionResult insert_result = executor.execute(
            parser.parse("INSERT INTO executor_drop_users (name) VALUES ('Alice');")
        );

        assert(insert_result.success);
        assert(insert_result.error_message.empty());
        assert(insert_result.affected_rows == 1);
    }

    {
        const db::execution::ExecutionResult drop_result = executor.execute(
            parser.parse("DROP TABLE executor_drop_users;")
        );

        assert(drop_result.success);
        assert(drop_result.error_message.empty());
        assert(drop_result.affected_rows == 0);
        assert(!catalog.has_table("executor_drop_users"));
        assert(!std::filesystem::exists(heap_file_name));
        assert(!std::filesystem::exists(primary_index_file_name));
    }

    {
        const db::execution::ExecutionResult select_result = executor.execute(
            parser.parse("SELECT * FROM executor_drop_users;")
        );

        assert(!select_result.success);
        assert(!select_result.error_message.empty());
    }

    {
        const db::execution::ExecutionResult drop_missing_result = executor.execute(
            parser.parse("DROP TABLE executor_drop_users;")
        );

        assert(!drop_missing_result.success);
        assert(!drop_missing_result.error_message.empty());
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);

    std::cout << "Executor drop table test passed.\n";
    return 0;
}
