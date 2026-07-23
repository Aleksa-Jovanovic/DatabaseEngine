#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "sql/ast.h"
#include "table/row.h"

namespace db::execution {

struct ExecutionResult {
    bool success = false;
    std::string error_message;
    std::vector<std::string> column_names;
    std::vector<table::Row> rows;
    std::size_t affected_rows = 0;
};

class Executor {
public:
    // data_directory scopes new table files into data_directory/<table>/.
    // An empty directory keeps the flat file layout used by low-level tests.
    explicit Executor(catalog::Catalog& catalog, std::string data_directory = "");

    ExecutionResult execute(const sql::Statement& statement);

    // Execute many already-parsed INSERT statements for the same table through
    // one executor path. This keeps the actual row writes the same, but avoids
    // repeating per-statement overhead such as catalog lookups, table opening,
    // executor dispatch, and response creation for every inserted row.
    ExecutionResult execute_insert_batch(
        const std::vector<sql::InsertStatement>& insert_statements
    );

private:
    catalog::Catalog& catalog_;
    std::string data_directory_;

    // Temporary one-entry table cache. A future TableManager should own opened
    // runtime table objects and handle invalidation outside the executor.
    std::string cached_table_name_;
    std::unique_ptr<table::Table> cached_table_;

    table::Table* open_cached_table(const std::string& table_name);
    void clear_cached_table();

    ExecutionResult execute_create_table(const sql::CreateTableStatement& statement);
    ExecutionResult execute_drop_table(const sql::DropTableStatement& statement);
    ExecutionResult execute_create_index(const sql::CreateIndexStatement& statement);
    ExecutionResult execute_drop_index(const sql::DropIndexStatement& statement);
    ExecutionResult execute_select(const sql::SelectStatement& statement);
    ExecutionResult execute_insert(const sql::InsertStatement& statement);
    ExecutionResult execute_update(const sql::UpdateStatement& statement);
    ExecutionResult execute_delete(const sql::DeleteStatement& statement);
};

}  // namespace db::execution
