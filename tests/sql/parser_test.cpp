#include <cassert>
#include <iostream>
#include <string>
#include <variant>

#include "sql/ast.h"
#include "sql/parser.h"
#include "sql/tokenizer.h"

namespace {

const db::sql::WhereExpression& require_where_expression(
    const db::sql::SelectStatement& select
) {
    assert(select.where_expression.has_value());
    return select.where_expression.value();
}

const db::sql::ComparisonExpression& require_comparison(
    const db::sql::WhereExpression& expression
) {
    assert(expression.kind == db::sql::WhereExpressionKind::Comparison);
    assert(expression.comparison.has_value());
    return expression.comparison.value();
}

const db::sql::BetweenExpression& require_between(
    const db::sql::WhereExpression& expression
) {
    assert(expression.kind == db::sql::WhereExpressionKind::Between);
    assert(expression.between.has_value());
    return expression.between.value();
}

const db::sql::WhereExpression& require_logical_child(
    const db::sql::WhereExpression& expression,
    db::sql::LogicalOperator expected_operator,
    bool left_child
) {
    assert(expression.kind == db::sql::WhereExpressionKind::Logical);
    assert(expression.logical_operator.has_value());
    assert(expression.logical_operator.value() == expected_operator);

    if (left_child) {
        assert(expression.left != nullptr);
        return *expression.left;
    }

    assert(expression.right != nullptr);
    return *expression.right;
}

}  // namespace

int main() {
    db::sql::Parser parser;

    {
        const db::sql::Statement statement =
            parser.parse("CREATE TABLE users (id INTEGER, name STRING);");

        assert(std::holds_alternative<db::sql::CreateTableStatement>(statement));

        const auto& create_table =
            std::get<db::sql::CreateTableStatement>(statement);

        assert(create_table.table_name == "users");
        assert(create_table.columns.size() == 2);

        assert(create_table.columns[0].name == "id");
        assert(create_table.columns[0].type == db::sql::SqlTypeName::Integer);

        assert(create_table.columns[1].name == "name");
        assert(create_table.columns[1].type == db::sql::SqlTypeName::String);
    }

    {
        const db::sql::Statement statement =
            parser.parse("CREATE TABLE flags (enabled BOOLEAN, created_at DATE);");

        assert(std::holds_alternative<db::sql::CreateTableStatement>(statement));

        const auto& create_table =
            std::get<db::sql::CreateTableStatement>(statement);

        assert(create_table.table_name == "flags");
        assert(create_table.columns.size() == 2);

        assert(create_table.columns[0].name == "enabled");
        assert(create_table.columns[0].type == db::sql::SqlTypeName::Boolean);

        assert(create_table.columns[1].name == "created_at");
        assert(create_table.columns[1].type == db::sql::SqlTypeName::Date);
    }

    {
        const db::sql::Statement statement =
            parser.parse("INSERT INTO users VALUES (1, 'Alice', TRUE, DATE '2026-06-17');");

        assert(std::holds_alternative<db::sql::InsertStatement>(statement));

        const auto& insert =
            std::get<db::sql::InsertStatement>(statement);

        assert(insert.table_name == "users");
        assert(insert.column_names.empty());
        assert(insert.values.size() == 4);

        assert(std::get<db::sql::IntegerLiteral>(insert.values[0]).value == 1);
        assert(std::get<db::sql::StringLiteral>(insert.values[1]).value == "Alice");
        assert(std::get<db::sql::BooleanLiteral>(insert.values[2]).value == true);
        assert(std::get<db::sql::DateLiteral>(insert.values[3]).value == "2026-06-17");
    }

    {
        const db::sql::Statement statement =
            parser.parse("INSERT INTO users (id, name, active, created_at) VALUES (1, 'Alice', TRUE, DATE '2026-06-17');");

        assert(std::holds_alternative<db::sql::InsertStatement>(statement));

        const auto& insert =
            std::get<db::sql::InsertStatement>(statement);

        assert(insert.table_name == "users");
        assert(insert.column_names.size() == 4);
        assert(insert.column_names[0] == "id");
        assert(insert.column_names[1] == "name");
        assert(insert.column_names[2] == "active");
        assert(insert.column_names[3] == "created_at");

        assert(insert.values.size() == 4);
        assert(std::get<db::sql::IntegerLiteral>(insert.values[0]).value == 1);
        assert(std::get<db::sql::StringLiteral>(insert.values[1]).value == "Alice");
        assert(std::get<db::sql::BooleanLiteral>(insert.values[2]).value == true);
        assert(std::get<db::sql::DateLiteral>(insert.values[3]).value == "2026-06-17");
    }

    {
        bool threw = false;
        try {
            parser.parse("CREATE TABLE users (id INTEGER, name STRING)");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Unexpected token type") != std::string::npos ||
                   std::string(ex.what()).find("Unexpected end of token stream") != std::string::npos);
        }

        assert(threw);
    }

    {
        bool threw = false;
        try {
            parser.parse("CREATE TABLE users (id, name STRING);");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Expected SQL type name token") != std::string::npos ||
                   std::string(ex.what()).find("Unexpected token type") != std::string::npos);
        }

        assert(threw);
    }

    {
        bool threw = false;
        try {
            parser.parse("INSERT INTO users () VALUES (1);");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("INSERT column list must define at least one column") != std::string::npos);
        }

        assert(threw);
    }

    {
        bool threw = false;
        try {
            parser.parse("INSERT INTO users (id, name) VALUES ();");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("INSERT must define at least one value") != std::string::npos);
        }

        assert(threw);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == true);
        assert(select.column_names.empty());
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT id, name FROM users;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == false);
        assert(select.column_names.size() == 2);
        assert(select.column_names[0] == "id");
        assert(select.column_names[1] == "name");
    }

    {
        bool threw = false;
        try {
            parser.parse("SELECT FROM users;");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Unexpected token type") != std::string::npos);
        }

        assert(threw);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id = 1;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == true);
        assert(select.column_names.empty());

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "id");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::Equal);
        assert(std::get<db::sql::IntegerLiteral>(comparison.value).value == 1);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT id, name FROM users WHERE name = 'Alice';");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == false);
        assert(select.column_names.size() == 2);

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "name");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::Equal);
        assert(std::get<db::sql::StringLiteral>(comparison.value).value == "Alice");
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id < 10;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == true);

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "id");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::LessThan);
        assert(std::get<db::sql::IntegerLiteral>(comparison.value).value == 10);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id <= 10;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "id");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::LessThanOrEqual);
        assert(std::get<db::sql::IntegerLiteral>(comparison.value).value == 10);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT id, name FROM users WHERE id > 10;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");
        assert(select.select_all == false);
        assert(select.column_names.size() == 2);

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "id");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::GreaterThan);
        assert(std::get<db::sql::IntegerLiteral>(comparison.value).value == 10);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id >= 10;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        assert(select.table_name == "users");

        const auto& comparison = require_comparison(require_where_expression(select));
        assert(comparison.column_name == "id");
        assert(comparison.comparison_operator == db::sql::ComparisonOperator::GreaterThanOrEqual);
        assert(std::get<db::sql::IntegerLiteral>(comparison.value).value == 10);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id = 1 AND active = TRUE;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        const auto& where = require_where_expression(select);
        const auto& left = require_logical_child(where, db::sql::LogicalOperator::And, true);
        const auto& right = require_logical_child(where, db::sql::LogicalOperator::And, false);

        const auto& left_comparison = require_comparison(left);
        assert(left_comparison.column_name == "id");
        assert(left_comparison.comparison_operator == db::sql::ComparisonOperator::Equal);
        assert(std::get<db::sql::IntegerLiteral>(left_comparison.value).value == 1);

        const auto& right_comparison = require_comparison(right);
        assert(right_comparison.column_name == "active");
        assert(right_comparison.comparison_operator == db::sql::ComparisonOperator::Equal);
        assert(std::get<db::sql::BooleanLiteral>(right_comparison.value).value == true);
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE name = 'Alice' OR name = 'Bob';");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        const auto& where = require_where_expression(select);
        const auto& left = require_logical_child(where, db::sql::LogicalOperator::Or, true);
        const auto& right = require_logical_child(where, db::sql::LogicalOperator::Or, false);

        const auto& left_comparison = require_comparison(left);
        assert(left_comparison.column_name == "name");
        assert(std::get<db::sql::StringLiteral>(left_comparison.value).value == "Alice");

        const auto& right_comparison = require_comparison(right);
        assert(right_comparison.column_name == "name");
        assert(std::get<db::sql::StringLiteral>(right_comparison.value).value == "Bob");
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id = 1 OR active = TRUE AND name = 'Alice';");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        const auto& where = require_where_expression(select);
        const auto& left = require_logical_child(where, db::sql::LogicalOperator::Or, true);
        const auto& right = require_logical_child(where, db::sql::LogicalOperator::Or, false);

        const auto& left_comparison = require_comparison(left);
        assert(left_comparison.column_name == "id");

        const auto& and_left = require_logical_child(right, db::sql::LogicalOperator::And, true);
        const auto& and_right = require_logical_child(right, db::sql::LogicalOperator::And, false);

        const auto& active_comparison = require_comparison(and_left);
        assert(active_comparison.column_name == "active");

        const auto& name_comparison = require_comparison(and_right);
        assert(name_comparison.column_name == "name");
    }

    {
        const db::sql::Statement statement =
            parser.parse("SELECT * FROM users WHERE id BETWEEN 10 AND 20;");

        assert(std::holds_alternative<db::sql::SelectStatement>(statement));

        const auto& select =
            std::get<db::sql::SelectStatement>(statement);

        const auto& between = require_between(require_where_expression(select));
        assert(between.column_name == "id");
        assert(std::get<db::sql::IntegerLiteral>(between.lower_bound).value == 10);
        assert(std::get<db::sql::IntegerLiteral>(between.upper_bound).value == 20);
    }

    {
        const db::sql::Statement statement =
            parser.parse("DELETE FROM users;");

        assert(std::holds_alternative<db::sql::DeleteStatement>(statement));

        const auto& delete_statement =
            std::get<db::sql::DeleteStatement>(statement);

        assert(delete_statement.table_name == "users");
        assert(!delete_statement.where_expression.has_value());
    }

    {
        const db::sql::Statement statement =
            parser.parse("DELETE FROM users WHERE id = 1 OR active = TRUE;");

        assert(std::holds_alternative<db::sql::DeleteStatement>(statement));

        const auto& delete_statement =
            std::get<db::sql::DeleteStatement>(statement);

        assert(delete_statement.table_name == "users");

        const auto& where = delete_statement.where_expression.value();
        const auto& left = require_logical_child(where, db::sql::LogicalOperator::Or, true);
        const auto& right = require_logical_child(where, db::sql::LogicalOperator::Or, false);

        const auto& left_comparison = require_comparison(left);
        assert(left_comparison.column_name == "id");
        assert(std::get<db::sql::IntegerLiteral>(left_comparison.value).value == 1);

        const auto& right_comparison = require_comparison(right);
        assert(right_comparison.column_name == "active");
        assert(std::get<db::sql::BooleanLiteral>(right_comparison.value).value == true);
    }

    {
        bool threw = false;
        try {
            parser.parse("DELETE users WHERE id = 1;");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Unexpected token type") != std::string::npos ||
                   std::string(ex.what()).find("Unexpected token lexeme") != std::string::npos);
        }

        assert(threw);
    }

    {
        bool threw = false;
        try {
            parser.parse("SELECT * FROM users WHERE id;");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Expected comparison operator") != std::string::npos);
        }

        assert(threw);
    }

    std::cout << "Parser test passed.\n";
    return 0;
}
