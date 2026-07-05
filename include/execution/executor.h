#pragma once

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
};

class Executor {
public:
    explicit Executor(catalog::Catalog& catalog);

    ExecutionResult execute(const sql::Statement& statement);
    ExecutionResult execute_select(const sql::SelectStatement& select_statement);

private:
    catalog::Catalog& catalog_;
};

}  // namespace db::execution
