#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "execution/executor.h"
#include "sql/parser.h"

namespace db::server {

struct QueryResponse {
    bool success = false;
    std::string error_message;
    std::vector<std::string> column_names;
    std::vector<std::vector<std::string>> rows;
    std::size_t affected_rows = 0;
};

class DatabaseServer {
public:
    DatabaseServer();

    // Execute one SQL statement and return a frontend-friendly response.
    QueryResponse execute_query(const std::string& sql);

    // Placeholder for the future HTTP listener.
    // The next step will wire this to a local web endpoint such as POST /query.
    bool start(const std::string& host, int port);

private:
    catalog::Catalog catalog_;
    sql::Parser parser_;
    execution::Executor executor_;

    static QueryResponse to_query_response(
        const execution::ExecutionResult& execution_result
    );
};

}  // namespace db::server
