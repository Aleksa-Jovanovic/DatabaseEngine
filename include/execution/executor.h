#pragma once

#include <cstddef>
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
    explicit Executor(catalog::Catalog& catalog);

    ExecutionResult execute(const sql::Statement& statement);

private:
    catalog::Catalog& catalog_;

    ExecutionResult execute_create_table(const sql::CreateTableStatement& statement);
    ExecutionResult execute_drop_table(const sql::DropTableStatement& statement);
    ExecutionResult execute_select(const sql::SelectStatement& statement);
    ExecutionResult execute_insert(const sql::InsertStatement& statement);
    ExecutionResult execute_update(const sql::UpdateStatement& statement);
    ExecutionResult execute_delete(const sql::DeleteStatement& statement);
};

}  // namespace db::execution
