#pragma once

#include <cstdint>
#include <optional>
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

// SelectStatement - BEGIN
enum class ComparisonOperator {
    Equal,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual
};

struct WhereClause {
    std::string column_name;
    ComparisonOperator comparison_operator;
    ValueNode value;
};

struct SelectStatement {
    std::string table_name;
    bool select_all;
    std::vector<std::string> column_names;
    std::optional<WhereClause> where_clause;
};
// SelectStatement - END

using Statement = std::variant<
    CreateTableStatement,
    InsertStatement,
    SelectStatement
>;

}  // namespace db::sql
