#pragma once

#include <cstdint>
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

// CreateTableStatement - BEGIN
struct ColumnDefinitionNode {
    std::string name;
    SqlTypeName type;
};

struct CreateTableStatement {
    std::string table_name;
    std::vector<ColumnDefinitionNode> columns;
};
// CreateTableStatement - END

// InsertStatement - BEGIN
struct IntegerLiteral {
  std::int64_t value;
};

struct StringLiteral {
  std::string value;
};

struct BooleanLiteral {
    bool value;
};

struct DateLiteral {
    std::string value;
};

using ValueNode = std::variant<
    IntegerLiteral,
    StringLiteral,
    BooleanLiteral,
    DateLiteral
>;

struct InsertStatement {
    std::string table_name;
    std::vector<std::string> column_names;
    std::vector<ValueNode> values;
};
// InsertStatement - END

using Statement = std::variant<CreateTableStatement, InsertStatement>;

}  // namespace db::sql