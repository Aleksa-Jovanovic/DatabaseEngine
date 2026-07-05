#include "execution/executor.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace db::execution {
namespace {

std::optional<std::size_t> find_column_index(
    const catalog::TableDefinition& table_definition,
    const std::string& column_name
) {
    const auto& columns = table_definition.schema.columns();

    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == column_name) {
            return i;
        }
    }

    return std::nullopt;
}

std::vector<std::size_t> all_column_indexes(
    const catalog::TableDefinition& table_definition
) {
    std::vector<std::size_t> indexes;
    const auto& columns = table_definition.schema.columns();

    for (std::size_t i = 0; i < columns.size(); ++i) {
        indexes.push_back(i);
    }

    return indexes;
}

std::vector<std::string> column_names_for_indexes(
    const catalog::TableDefinition& table_definition,
    const std::vector<std::size_t>& column_indexes
) {
    std::vector<std::string> column_names;
    const auto& columns = table_definition.schema.columns();

    for (std::size_t column_index : column_indexes) {
        column_names.push_back(columns[column_index].name);
    }

    return column_names;
}

std::optional<std::vector<std::size_t>> resolve_projection_indexes(
    const catalog::TableDefinition& table_definition,
    const sql::SelectStatement& select_statement
) {
    if (select_statement.select_all) {
        return all_column_indexes(table_definition);
    }

    std::vector<std::size_t> projection_indexes;

    for (const std::string& column_name : select_statement.column_names) {
        const auto column_index = find_column_index(table_definition, column_name);
        if (!column_index.has_value()) {
            return std::nullopt;
        }

        projection_indexes.push_back(column_index.value());
    }

    return projection_indexes;
}

table::Row project_row(
    const table::Row& row,
    const std::vector<std::size_t>& column_indexes
) {
    table::Row projected_row;
    projected_row.key = row.key;

    for (std::size_t column_index : column_indexes) {
        if (column_index < row.values.size()) {
            projected_row.values.push_back(row.values[column_index]);
        }
    }

    return projected_row;
}

std::vector<table::Row> project_rows(
    const std::vector<table::Row>& rows,
    const std::vector<std::size_t>& column_indexes
) {
    std::vector<table::Row> projected_rows;

    for (const table::Row& row : rows) {
        projected_rows.push_back(project_row(row, column_indexes));
    }

    return projected_rows;
}

bool values_equal(
    const table::FieldValue& left,
    const sql::ValueNode& right
) {
    if (std::holds_alternative<std::int64_t>(left) &&
        std::holds_alternative<sql::IntegerLiteral>(right)) {
        return std::get<std::int64_t>(left) ==
               std::get<sql::IntegerLiteral>(right).value;
    }

    if (std::holds_alternative<std::string>(left) &&
        std::holds_alternative<sql::StringLiteral>(right)) {
        return std::get<std::string>(left) ==
               std::get<sql::StringLiteral>(right).value;
    }

    if (std::holds_alternative<bool>(left) &&
        std::holds_alternative<sql::BooleanLiteral>(right)) {
        return std::get<bool>(left) ==
               std::get<sql::BooleanLiteral>(right).value;
    }

    if (std::holds_alternative<table::DateValue>(left) &&
        std::holds_alternative<sql::DateLiteral>(right)) {
        return std::get<table::DateValue>(left).value ==
               std::get<sql::DateLiteral>(right).value;
    }

    return false;
}

std::optional<int> compare_values(
    const table::FieldValue& left,
    const sql::ValueNode& right
) {
    if (std::holds_alternative<std::int64_t>(left) &&
        std::holds_alternative<sql::IntegerLiteral>(right)) {
        const auto left_value = std::get<std::int64_t>(left);
        const auto right_value = std::get<sql::IntegerLiteral>(right).value;

        if (left_value < right_value) {
            return -1;
        }

        if (left_value > right_value) {
            return 1;
        }

        return 0;
    }

    if (std::holds_alternative<std::string>(left) &&
        std::holds_alternative<sql::StringLiteral>(right)) {
        const auto& left_value = std::get<std::string>(left);
        const auto& right_value = std::get<sql::StringLiteral>(right).value;

        if (left_value < right_value) {
            return -1;
        }

        if (left_value > right_value) {
            return 1;
        }

        return 0;
    }

    if (std::holds_alternative<table::DateValue>(left) &&
        std::holds_alternative<sql::DateLiteral>(right)) {
        const auto& left_value = std::get<table::DateValue>(left).value;
        const auto& right_value = std::get<sql::DateLiteral>(right).value;

        if (left_value < right_value) {
            return -1;
        }

        if (left_value > right_value) {
            return 1;
        }

        return 0;
    }

    return std::nullopt;
}

bool evaluate_comparison_operator(
    const table::FieldValue& left,
    sql::ComparisonOperator comparison_operator,
    const sql::ValueNode& right
) {
    if (comparison_operator == sql::ComparisonOperator::Equal) {
        return values_equal(left, right);
    }

    const auto comparison = compare_values(left, right);
    if (!comparison.has_value()) {
        return false;
    }

    switch (comparison_operator) {
        case sql::ComparisonOperator::LessThan:
            return comparison.value() < 0;

        case sql::ComparisonOperator::LessThanOrEqual:
            return comparison.value() <= 0;

        case sql::ComparisonOperator::GreaterThan:
            return comparison.value() > 0;

        case sql::ComparisonOperator::GreaterThanOrEqual:
            return comparison.value() >= 0;

        case sql::ComparisonOperator::Equal:
            return comparison.value() == 0;
    }

    return false;
}

bool evaluate_comparison_expression(
    const catalog::TableDefinition& table_definition,
    const table::Row& row,
    const sql::ComparisonExpression& comparison
) {
    const auto column_index =
        find_column_index(table_definition, comparison.column_name);

    if (!column_index.has_value()) {
        return false;
    }

    if (column_index.value() >= row.values.size()) {
        return false;
    }

    return evaluate_comparison_operator(
        row.values[column_index.value()],
        comparison.comparison_operator,
        comparison.value
    );
}

bool evaluate_between_expression(
    const catalog::TableDefinition& table_definition,
    const table::Row& row,
    const sql::BetweenExpression& between
) {
    const auto column_index =
        find_column_index(table_definition, between.column_name);

    if (!column_index.has_value()) {
        return false;
    }

    if (column_index.value() >= row.values.size()) {
        return false;
    }

    const table::FieldValue& field_value = row.values[column_index.value()];

    return evaluate_comparison_operator(
               field_value,
               sql::ComparisonOperator::GreaterThanOrEqual,
               between.lower_bound
           ) &&
           evaluate_comparison_operator(
               field_value,
               sql::ComparisonOperator::LessThanOrEqual,
               between.upper_bound
           );
}

bool evaluate_where_expression(
    const catalog::TableDefinition& table_definition,
    const table::Row& row,
    const sql::WhereExpression& expression
) {
    switch (expression.kind) {
        case sql::WhereExpressionKind::Comparison:
            if (!expression.comparison.has_value()) {
                return false;
            }

            return evaluate_comparison_expression(
                table_definition,
                row,
                expression.comparison.value()
            );

        case sql::WhereExpressionKind::Between:
            if (!expression.between.has_value()) {
                return false;
            }

            return evaluate_between_expression(
                table_definition,
                row,
                expression.between.value()
            );

        case sql::WhereExpressionKind::Logical:
            if (!expression.logical_operator.has_value() ||
                expression.left == nullptr ||
                expression.right == nullptr) {
                return false;
            }

            if (expression.logical_operator.value() == sql::LogicalOperator::And) {
                return evaluate_where_expression(
                           table_definition,
                           row,
                           *expression.left
                       ) &&
                       evaluate_where_expression(
                           table_definition,
                           row,
                           *expression.right
                       );
            }

            if (expression.logical_operator.value() == sql::LogicalOperator::Or) {
                return evaluate_where_expression(
                           table_definition,
                           row,
                           *expression.left
                       ) ||
                       evaluate_where_expression(
                           table_definition,
                           row,
                           *expression.right
                       );
            }

            return false;
    }

    return false;
}

std::vector<table::Row> filter_rows(
    const catalog::TableDefinition& table_definition,
    const std::vector<table::Row>& rows,
    const std::optional<sql::WhereExpression>& where_expression
) {
    if (!where_expression.has_value()) {
        return rows;
    }

    std::vector<table::Row> filtered_rows;

    for (const table::Row& row : rows) {
        if (evaluate_where_expression(
                table_definition,
                row,
                where_expression.value()
            )) {
            filtered_rows.push_back(row);
        }
    }

    return filtered_rows;
}

} // namespace

Executor::Executor(catalog::Catalog& catalog)
    : catalog_(catalog) {}

ExecutionResult Executor::execute(const sql::Statement& statement) {
    if (std::holds_alternative<sql::SelectStatement>(statement)) {
        return execute_select(std::get<sql::SelectStatement>(statement));
    }

    return ExecutionResult{
        false,
        "Statement execution is not supported yet",
        {},
        {}
    };
}

ExecutionResult Executor::execute_select(const sql::SelectStatement& select_statement) {
    const auto table_definition = catalog_.find_table_definition(select_statement.table_name);
    if (!table_definition.has_value()) {
        return ExecutionResult{
            false,
            "Table does not exist: " + select_statement.table_name,
            {},
            {}
        };
    }

    const auto projection_indexes =
        resolve_projection_indexes(table_definition.value(), select_statement);
    if (!projection_indexes.has_value()) {
        return ExecutionResult{
            false,
            "Unknown column in SELECT projection",
            {},
            {}
        };
    }

    auto table = catalog_.open_table(select_statement.table_name);
    if (table == nullptr) {
        return ExecutionResult{
            false,
            "Could not open table: " + select_statement.table_name,
            {},
            {}
        };
    }

    std::vector<table::Row> rows = table->scan();
    rows = filter_rows(
        table_definition.value(),
        rows,
        select_statement.where_expression
    );

    return ExecutionResult{
        true,
        "",
        column_names_for_indexes(table_definition.value(), projection_indexes.value()),
        project_rows(rows, projection_indexes.value())
    };
}

}  // namespace db::execution