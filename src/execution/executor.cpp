#include "execution/executor.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace db::execution {
namespace {

// SELECT STATEMENT

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

// INSERT STATEMENT

table::FieldValue value_node_to_field_value(const sql::ValueNode& value) {
    if (std::holds_alternative<sql::IntegerLiteral>(value)) {
        return std::get<sql::IntegerLiteral>(value).value;
    }

    if (std::holds_alternative<sql::StringLiteral>(value)) {
        return std::get<sql::StringLiteral>(value).value;
    }

    if (std::holds_alternative<sql::BooleanLiteral>(value)) {
        return std::get<sql::BooleanLiteral>(value).value;
    }

    if (std::holds_alternative<sql::DateLiteral>(value)) {
        return table::DateValue{std::get<sql::DateLiteral>(value).value};
    }

    return std::int64_t{0};
}

std::optional<std::size_t> find_primary_key_index(
    const catalog::TableDefinition& table_definition
) {
    const auto& columns = table_definition.schema.columns();

    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].is_primary_key) {
            return i;
        }
    }

    return std::nullopt;
}

table::FieldValue default_value_for_column(
    const catalog::ColumnDefinition& column
) {
    switch (column.type) {
        case catalog::ColumnType::Integer:
            return std::int64_t{0};

        case catalog::ColumnType::String:
            return std::string{};

        case catalog::ColumnType::Boolean:
            return false;

        case catalog::ColumnType::Date:
            return table::DateValue{""};
    }

    return std::int64_t{0};
}

std::vector<table::FieldValue> default_values_for_schema(
    const catalog::TableDefinition& table_definition
) {
    std::vector<table::FieldValue> values;

    for (const auto& column : table_definition.schema.columns()) {
        values.push_back(default_value_for_column(column));
    }

    return values;
}

bool primary_key_is_auto_increment(
    const catalog::TableDefinition& table_definition
) {
    const auto primary_key_index = find_primary_key_index(table_definition);
    if (!primary_key_index.has_value()) {
        return false;
    }

    const auto& columns = table_definition.schema.columns();
    if (primary_key_index.value() >= columns.size()) {
        return false;
    }

    return columns[primary_key_index.value()].is_auto_increment;
}

bool insert_provides_primary_key(
    const catalog::TableDefinition& table_definition,
    const sql::InsertStatement& insert_statement
) {
    const auto primary_key_index = find_primary_key_index(table_definition);
    if (!primary_key_index.has_value()) {
        return false;
    }

    if (insert_statement.column_names.empty()) {
        return insert_statement.values.size() == table_definition.schema.columns().size();
    }

    for (const std::string& column_name : insert_statement.column_names) {
        const auto column_index = find_column_index(table_definition, column_name);
        if (!column_index.has_value()) {
            return false;
        }

        if (column_index.value() == primary_key_index.value()) {
            return true;
        }
    }

    return false;
}

struct InsertValuesResult {
    std::vector<table::FieldValue> values;
    bool primary_key_was_provided = false;
};

std::optional<InsertValuesResult> build_schema_ordered_values(
    const catalog::TableDefinition& table_definition,
    const sql::InsertStatement& insert_statement
) {
    const auto& columns = table_definition.schema.columns();

    InsertValuesResult result;
    result.values = default_values_for_schema(table_definition);

    if (insert_statement.column_names.empty()) {
        const auto primary_key_index = find_primary_key_index(table_definition);
        if (!primary_key_index.has_value()) {
            return std::nullopt;
        }

        if (insert_statement.values.size() == columns.size()) {
            result.primary_key_was_provided = true;

            for (std::size_t i = 0; i < insert_statement.values.size(); ++i) {
                result.values[i] = value_node_to_field_value(insert_statement.values[i]);
            }

            return result;
        }

        if (insert_statement.values.size() == columns.size() - 1) {
            result.primary_key_was_provided = false;

            std::size_t source_index = 0;

            for (std::size_t column_index = 0; column_index < columns.size(); ++column_index) {
                if (column_index == primary_key_index.value()) {
                    continue;
                }

                result.values[column_index] =
                    value_node_to_field_value(insert_statement.values[source_index]);

                ++source_index;
            }

            return result;
        }

        return std::nullopt;
    }

    if (insert_statement.column_names.size() != insert_statement.values.size()) {
        return std::nullopt;
    }

    std::vector<bool> assigned(columns.size(), false);

    for (std::size_t i = 0; i < insert_statement.column_names.size(); ++i) {
        const auto column_index =
            find_column_index(table_definition, insert_statement.column_names[i]);

        if (!column_index.has_value()) {
            return std::nullopt;
        }

        if (assigned[column_index.value()]) {
            return std::nullopt;
        }

        result.values[column_index.value()] =
            value_node_to_field_value(insert_statement.values[i]);

        assigned[column_index.value()] = true;
    }

    const auto primary_key_index = find_primary_key_index(table_definition);
    if (!primary_key_index.has_value()) {
        return std::nullopt;
    }

    result.primary_key_was_provided = assigned[primary_key_index.value()];
    return result;
}

std::optional<table::Row> build_row_from_insert(
    const catalog::TableDefinition& table_definition,
    const sql::InsertStatement& insert_statement,
    std::optional<std::uint32_t> generated_primary_key
) {
    const auto insert_values =
        build_schema_ordered_values(table_definition, insert_statement);

    if (!insert_values.has_value()) {
        return std::nullopt;
    }

    const auto primary_key_index = find_primary_key_index(table_definition);
    if (!primary_key_index.has_value()) {
        return std::nullopt;
    }

    std::vector<table::FieldValue> values = insert_values->values;

    if (!insert_values->primary_key_was_provided) {
        if (!generated_primary_key.has_value()) {
            return std::nullopt;
        }

        values[primary_key_index.value()] =
            std::int64_t{generated_primary_key.value()};
    }

    const table::FieldValue& primary_key_value =
        values[primary_key_index.value()];

    if (!std::holds_alternative<std::int64_t>(primary_key_value)) {
        return std::nullopt;
    }

    const std::int64_t key = std::get<std::int64_t>(primary_key_value);
    if (key < 0) {
        return std::nullopt;
    }

    table::Row row;
    row.key = static_cast<std::uint32_t>(key);
    row.values = std::move(values);

    return row;
}

// UPDATE STATEMENT

bool assignment_targets_primary_key(
    const catalog::TableDefinition& table_definition,
    const sql::AssignmentExpression& assignment
) {
    const auto primary_key_index = find_primary_key_index(table_definition);
    if (!primary_key_index.has_value()) {
        return false;
    }

    const auto column_index =
        find_column_index(table_definition, assignment.column_name);

    if (!column_index.has_value()) {
        return false;
    }

    return column_index.value() == primary_key_index.value();
}

bool update_changes_primary_key(
    const catalog::TableDefinition& table_definition,
    const sql::UpdateStatement& update_statement
) {
    for (const sql::AssignmentExpression& assignment : update_statement.assignments) {
        if (assignment_targets_primary_key(table_definition, assignment)) {
            return true;
        }
    }

    return false;
}

std::optional<table::Row> apply_assignments_to_row(
    const catalog::TableDefinition& table_definition,
    const table::Row& row,
    const std::vector<sql::AssignmentExpression>& assignments
) {
    table::Row updated_row = row;

    for (const sql::AssignmentExpression& assignment : assignments) {
        const auto column_index =
            find_column_index(table_definition, assignment.column_name);

        if (!column_index.has_value()) {
            return std::nullopt;
        }

        if (column_index.value() >= updated_row.values.size()) {
            return std::nullopt;
        }

        updated_row.values[column_index.value()] =
            value_node_to_field_value(assignment.value);
    }

    return updated_row;
}

// CREATE TABLE STATEMENT

catalog::ColumnType to_catalog_column_type(sql::SqlTypeName type) {
    switch (type) {
        case sql::SqlTypeName::Integer:
            return catalog::ColumnType::Integer;
        case sql::SqlTypeName::String:
            return catalog::ColumnType::String;
        case sql::SqlTypeName::Boolean:
            return catalog::ColumnType::Boolean;
        case sql::SqlTypeName::Date:
            return catalog::ColumnType::Date;
    }

    return catalog::ColumnType::String;
}

std::string heap_file_name_for_table(const std::string& table_name) {
    return table_name + "_heap.db";
}

std::string primary_index_file_name_for_table(const std::string& table_name) {
    return table_name + "_primary_index.db";
}

std::string primary_index_name_for_table(const std::string& table_name) {
    return table_name + "_pkey";
}

std::optional<sql::ColumnDefinitionNode> find_primary_key_column(
    const sql::CreateTableStatement& create_table_statement
) {
    for (const sql::ColumnDefinitionNode& column : create_table_statement.columns) {
        if (column.is_primary_key) {
            return column;
        }
    }

    return std::nullopt;
}

catalog::TableDefinition build_table_definition_from_create(
    const sql::CreateTableStatement& create_table_statement
) {
    catalog::Schema schema;

    for (const sql::ColumnDefinitionNode& column : create_table_statement.columns) {
        schema.add_column(catalog::ColumnDefinition{
            column.name,
            to_catalog_column_type(column.type),
            column.is_primary_key,
            column.is_auto_increment
        });
    }

    const auto primary_key_column = find_primary_key_column(create_table_statement);

    return catalog::TableDefinition{
        create_table_statement.table_name,
        schema,
        create_table_statement.table_name + "_heap.db",
        {
            catalog::IndexDefinition{
                create_table_statement.table_name + "_pkey",
                create_table_statement.table_name,
                primary_key_column->name,
                create_table_statement.table_name + "_primary_index.db",
                true,
                true
            }
        }
    };
}

} // namespace

Executor::Executor(catalog::Catalog& catalog)
    : catalog_(catalog) {}

ExecutionResult Executor::execute(const sql::Statement& statement) {
    if (std::holds_alternative<sql::SelectStatement>(statement)) {
        return execute_select(std::get<sql::SelectStatement>(statement));
    }

    if (std::holds_alternative<sql::InsertStatement>(statement)) {
        return execute_insert(std::get<sql::InsertStatement>(statement));
    }

    if (std::holds_alternative<sql::UpdateStatement>(statement)) {
        return execute_update(std::get<sql::UpdateStatement>(statement));
    }

    if (std::holds_alternative<sql::DeleteStatement>(statement)) {
        return execute_delete(std::get<sql::DeleteStatement>(statement));
    }

    if (std::holds_alternative<sql::CreateTableStatement>(statement)) {
        return execute_create_table(std::get<sql::CreateTableStatement>(statement));
    }

    return ExecutionResult{
        false,
        "Statement execution is not supported yet",
        {},
        {},
        0
    };
}

ExecutionResult Executor::execute_select(const sql::SelectStatement& select_statement) {
    const auto table_definition = 
        catalog_.find_table_definition(select_statement.table_name);
    
    if (!table_definition.has_value()) {
        return ExecutionResult{
            false,
            "Table does not exist: " + select_statement.table_name,
            {},
            {},
            0
        };
    }

    const auto projection_indexes =
        resolve_projection_indexes(table_definition.value(), select_statement);
    if (!projection_indexes.has_value()) {
        return ExecutionResult{
            false,
            "Unknown column in SELECT projection",
            {},
            {},
            0
        };
    }

    auto table = catalog_.open_table(select_statement.table_name);
    if (table == nullptr) {
        return ExecutionResult{
            false,
            "Could not open table: " + select_statement.table_name,
            {},
            {},
            0
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
        project_rows(rows, projection_indexes.value()),
        0
    };
}

ExecutionResult Executor::execute_insert(
    const sql::InsertStatement& insert_statement
) {
    const auto table_definition =
        catalog_.find_table_definition(insert_statement.table_name);

    if (!table_definition.has_value()) {
        return ExecutionResult{
            false,
            "Table does not exist: " + insert_statement.table_name,
            {},
            {},
            0
        };
    }

    auto table = catalog_.open_table(insert_statement.table_name);
    if (table == nullptr) {
        return ExecutionResult{
            false,
            "Could not open table: " + insert_statement.table_name,
            {},
            {},
            0
        };
    }

    const bool primary_key_was_provided =
        insert_provides_primary_key(table_definition.value(), insert_statement);

    std::optional<std::uint32_t> generated_primary_key = std::nullopt;

    if (!primary_key_was_provided) {
        if (!primary_key_is_auto_increment(table_definition.value())) {
            return ExecutionResult{
                false,
                "INSERT must provide a PRIMARY KEY value",
                {},
                {},
                0
            };
        }

        generated_primary_key = table->allocate_primary_key();
    }

    const auto row =
        build_row_from_insert(
            table_definition.value(),
            insert_statement,
            generated_primary_key
        );

    if (!row.has_value()) {
        return ExecutionResult{
            false,
            "Could not build row from INSERT statement",
            {},
            {},
            0
        };
    }

    const auto row_id = table->insert(row.value());
    if (!row_id.has_value()) {
        return ExecutionResult{
            false,
            "INSERT failed",
            {},
            {},
            0
        };
    }

    return ExecutionResult{
        true,
        "",
        {},
        {},
        1
    };
}

ExecutionResult Executor::execute_update(
    const sql::UpdateStatement& update_statement
) {
    const auto table_definition =
        catalog_.find_table_definition(update_statement.table_name);

    if (!table_definition.has_value()) {
        return ExecutionResult{
            false,
            "Table does not exist: " + update_statement.table_name,
            {},
            {},
            0
        };
    }

    if (update_statement.assignments.empty()) {
        return ExecutionResult{
            false,
            "UPDATE must contain at least one assignment",
            {},
            {},
            0
        };
    }

    if (update_changes_primary_key(table_definition.value(), update_statement)) {
        return ExecutionResult{
            false,
            "Updating primary key is not supported yet",
            {},
            {},
            0
        };
    }

    auto table = catalog_.open_table(update_statement.table_name);
    if (table == nullptr) {
        return ExecutionResult{
            false,
            "Could not open table: " + update_statement.table_name,
            {},
            {},
            0
        };
    }

    const std::vector<table::Row> rows = table->scan();

    std::size_t updated_count = 0;

    for (const table::Row& row : rows) {
        bool should_update_row = true;

        if (update_statement.where_expression.has_value()) {
            should_update_row = evaluate_where_expression(
                table_definition.value(),
                row,
                update_statement.where_expression.value()
            );
        }

        if (!should_update_row) {
            continue;
        }

        const auto updated_row =
            apply_assignments_to_row(
                table_definition.value(),
                row,
                update_statement.assignments
            );

        if (!updated_row.has_value()) {
            return ExecutionResult{
                false,
                "Could not apply UPDATE assignment",
                {},
                {},
                updated_count
            };
        }

        if (!table->update_by_key(row.key, updated_row.value())) {
            return ExecutionResult{
                false,
                "UPDATE failed",
                {},
                {},
                updated_count
            };
        }

        ++updated_count;
    }

    return ExecutionResult{
        true,
        "",
        {},
        {},
        updated_count
    };
}

ExecutionResult Executor::execute_delete(
    const sql::DeleteStatement& delete_statement
) {
    const auto table_definition =
        catalog_.find_table_definition(delete_statement.table_name);

    if (!table_definition.has_value()) {
        return ExecutionResult{
            false,
            "Table does not exist: " + delete_statement.table_name,
            {},
            {},
            0
        };
    }

    auto table = catalog_.open_table(delete_statement.table_name);
    if (table == nullptr) {
        return ExecutionResult{
            false,
            "Could not open table: " + delete_statement.table_name,
            {},
            {},
            0
        };
    }

    const std::vector<table::Row> rows = table->scan();

    std::size_t deleted_count = 0;

    for (const table::Row& row : rows) {
        bool should_delete_row = true;

        if (delete_statement.where_expression.has_value()) {
            should_delete_row = evaluate_where_expression(
                table_definition.value(),
                row,
                delete_statement.where_expression.value()
            );
        }

        if (!should_delete_row) {
            continue;
        }

        if (!table->delete_by_key(row.key)) {
            return ExecutionResult{
                false,
                "DELETE failed",
                {},
                {},
                deleted_count
            };
        }

        ++deleted_count;
    }

    return ExecutionResult{
        true,
        "",
        {},
        {},
        deleted_count
    };
}

ExecutionResult Executor::execute_create_table(
    const sql::CreateTableStatement& create_table_statement
) {
    if (create_table_statement.table_name.empty()) {
        return ExecutionResult{false, "Table name cannot be empty", {}, {}, 0};
    }

    if (create_table_statement.columns.empty()) {
        return ExecutionResult{false, "CREATE TABLE must define at least one column", {}, {}, 0};
    }

    const auto primary_key_column = find_primary_key_column(create_table_statement);
    if (!primary_key_column.has_value()) {
        return ExecutionResult{false, "CREATE TABLE must define a PRIMARY KEY column", {}, {}, 0};
    }

    if (primary_key_column->type != sql::SqlTypeName::Integer) {
        return ExecutionResult{false, "PRIMARY KEY must be INTEGER", {}, {}, 0};
    }

    if (catalog_.has_table(create_table_statement.table_name)) {
        return ExecutionResult{
            false,
            "Table already exists: " + create_table_statement.table_name,
            {},
            {},
            0
        };
    }

    const catalog::TableDefinition table_definition =
        build_table_definition_from_create(create_table_statement);

    if (!catalog_.create_table(table_definition)) {
        return ExecutionResult{false, "CREATE TABLE failed validation", {}, {}, 0};
    }

    return ExecutionResult{true, "", {}, {}, 0};
}

}  // namespace db::execution
