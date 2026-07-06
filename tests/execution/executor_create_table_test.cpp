#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

#include "catalog/catalog.h"
#include "execution/executor.h"
#include "sql/parser.h"

int main() {
    const std::string users_heap_file_name = "users_heap.db";
    const std::string users_primary_index_file_name = "users_primary_index.db";
    const std::string citizens_heap_file_name = "citizens_heap.db";
    const std::string citizens_primary_index_file_name = "citizens_primary_index.db";
    const std::string accounts_heap_file_name = "accounts_heap.db";
    const std::string accounts_primary_index_file_name = "accounts_primary_index.db";
    const std::string missing_primary_heap_file_name = "missing_primary_heap.db";
    const std::string missing_primary_primary_index_file_name =
        "missing_primary_primary_index.db";

    std::filesystem::remove(users_heap_file_name);
    std::filesystem::remove(users_primary_index_file_name);
    std::filesystem::remove(citizens_heap_file_name);
    std::filesystem::remove(citizens_primary_index_file_name);
    std::filesystem::remove(accounts_heap_file_name);
    std::filesystem::remove(accounts_primary_index_file_name);
    std::filesystem::remove(missing_primary_heap_file_name);
    std::filesystem::remove(missing_primary_primary_index_file_name);

    db::catalog::Catalog catalog;
    db::sql::Parser parser;
    db::execution::Executor executor(catalog);

    {
        const db::execution::ExecutionResult create_result = executor.execute(
            parser.parse("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name STRING, active BOOLEAN);")
        );

        assert(create_result.success);
        assert(create_result.error_message.empty());
        assert(catalog.has_table("users"));

        const auto users_definition = catalog.find_table_definition("users");
        assert(users_definition.has_value());
        assert(users_definition->table_name == "users");
        assert(users_definition->heap_file_name == users_heap_file_name);
        assert(users_definition->schema.columns().size() == 3);

        assert(users_definition->schema.columns()[0].name == "id");
        assert(users_definition->schema.columns()[0].is_primary_key);
        assert(users_definition->schema.columns()[0].is_auto_increment);

        assert(users_definition->schema.columns()[1].name == "name");
        assert(!users_definition->schema.columns()[1].is_primary_key);
        assert(!users_definition->schema.columns()[1].is_auto_increment);

        assert(users_definition->indexes.size() == 1);
        assert(users_definition->indexes[0].index_name == "users_pkey");
        assert(users_definition->indexes[0].table_name == "users");
        assert(users_definition->indexes[0].column_name == "id");
        assert(users_definition->indexes[0].file_name == users_primary_index_file_name);
        assert(users_definition->indexes[0].is_primary);
        assert(users_definition->indexes[0].is_unique);
    }

    {
        const db::execution::ExecutionResult omitted_primary_insert_result =
            executor.execute(parser.parse("INSERT INTO users (name, active) VALUES ('Alice', TRUE);"));

        assert(omitted_primary_insert_result.success);
        assert(omitted_primary_insert_result.error_message.empty());
        assert(omitted_primary_insert_result.affected_rows == 1);
    }

    {
        const db::execution::ExecutionResult duplicate_create_result = executor.execute(
            parser.parse("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name STRING);")
        );

        assert(!duplicate_create_result.success);
        assert(!duplicate_create_result.error_message.empty());
    }

    {
        const db::execution::ExecutionResult create_result = executor.execute(
            parser.parse("CREATE TABLE citizens (name STRING, jmbg INTEGER PRIMARY KEY);")
        );

        assert(create_result.success);
        assert(create_result.error_message.empty());

        const auto citizens_definition = catalog.find_table_definition("citizens");
        assert(citizens_definition.has_value());
        assert(citizens_definition->schema.columns().size() == 2);
        assert(!citizens_definition->schema.columns()[0].is_primary_key);
        assert(citizens_definition->schema.columns()[1].name == "jmbg");
        assert(citizens_definition->schema.columns()[1].is_primary_key);
        assert(!citizens_definition->schema.columns()[1].is_auto_increment);
        assert(citizens_definition->indexes.size() == 1);
        assert(citizens_definition->indexes[0].column_name == "jmbg");

        const db::execution::ExecutionResult missing_primary_insert_result =
            executor.execute(parser.parse("INSERT INTO citizens (name) VALUES ('Aleksa');"));

        assert(!missing_primary_insert_result.success);
        assert(!missing_primary_insert_result.error_message.empty());

        const db::execution::ExecutionResult manual_primary_insert_result =
            executor.execute(parser.parse("INSERT INTO citizens VALUES ('Aleksa', 123456789);"));

        assert(manual_primary_insert_result.success);
        assert(manual_primary_insert_result.error_message.empty());
        assert(manual_primary_insert_result.affected_rows == 1);
    }

    {
        const db::execution::ExecutionResult missing_primary_result = executor.execute(
            parser.parse("CREATE TABLE missing_primary (id INTEGER, name STRING);")
        );

        assert(!missing_primary_result.success);
        assert(!missing_primary_result.error_message.empty());
    }

    {
        const db::execution::ExecutionResult string_primary_result = executor.execute(
            parser.parse("CREATE TABLE accounts (email STRING PRIMARY KEY, name STRING);")
        );

        assert(!string_primary_result.success);
        assert(!string_primary_result.error_message.empty());
    }

    std::filesystem::remove(users_heap_file_name);
    std::filesystem::remove(users_primary_index_file_name);
    std::filesystem::remove(citizens_heap_file_name);
    std::filesystem::remove(citizens_primary_index_file_name);
    std::filesystem::remove(accounts_heap_file_name);
    std::filesystem::remove(accounts_primary_index_file_name);
    std::filesystem::remove(missing_primary_heap_file_name);
    std::filesystem::remove(missing_primary_primary_index_file_name);

    std::cout << "Executor create table test passed.\n";
    return 0;
}
