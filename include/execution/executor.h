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
    
    ExecutionResult execute_select(const sql::SelectStatement& select_statement);
    ExecutionResult execute_insert(const sql::InsertStatement& insert_statement);
    ExecutionResult execute_update(const sql::UpdateStatement& update_statement);
    ExecutionResult execute_delete(const sql::DeleteStatement& delete_statement);

    ExecutionResult execute_create_table(const sql::CreateTableStatement& create_table_statement);

private:
    catalog::Catalog& catalog_;
};

}  // namespace db::execution
