#pragma once

#include <string>
#include <variant>
#include <vector>

namespace db::sql {

enum class SqlTypeName {
    Integer,
    String,
    Boolean,
    Date
};

struct ColumnDefinitionNode {
    std::string name;
    SqlTypeName type;
};

struct CreateTableStatement {
    std::string table_name;
    std::vector<ColumnDefinitionNode> columns;
};

using Statement = std::variant<CreateTableStatement>;

}  // namespace db::sql