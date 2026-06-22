#pragma once

#include <cstdint>
#include <memory>
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

// WhereExpression - BEGIN
enum class ComparisonOperator {
    Equal,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual
};

enum class LogicalOperator {
    And,
    Or
};

enum class WhereExpressionKind {
    Comparison,
    Between,
    Logical
};

struct ComparisonExpression {
    std::string column_name;
    ComparisonOperator comparison_operator;
    ValueNode value;
};

struct BetweenExpression {
    std::string column_name;
    ValueNode lower_bound;
    ValueNode upper_bound;
};

struct WhereExpression {
    WhereExpressionKind kind;
    std::optional<ComparisonExpression> comparison;
    std::optional<BetweenExpression> between;
    std::optional<LogicalOperator> logical_operator;
    std::unique_ptr<WhereExpression> left;
    std::unique_ptr<WhereExpression> right;
};
// WhereExpression - END

// SelectStatement - BEGIN
struct SelectStatement {
    std::string table_name;
    bool select_all;
    std::vector<std::string> column_names;
    std::optional<WhereExpression> where_expression;
};
// SelectStatement - END

// DeleteStatement - BEGIN
struct DeleteStatement {
    std::string table_name;
    std::optional<WhereExpression> where_expression;
};
// DeleteStatement - END

// UpdateStatement - BEGIN
struct AssignmentExpression {
    std::string column_name;
    ValueNode value;
};

struct UpdateStatement {
    std::string table_name;
    std::vector<AssignmentExpression> assignments;
    std::optional<WhereExpression> where_expression;
};
// UpdateStatement - END

using Statement = std::variant<
    CreateTableStatement,
    InsertStatement,
    SelectStatement,
    DeleteStatement,
    UpdateStatement
>;

}  // namespace db::sql
