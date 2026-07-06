#include "sql/parser.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace db::sql {
namespace {

std::string make_parser_error(const std::string& message, std::size_t token_index) {
    std::ostringstream out;
    out << message << " at token index " << token_index;
    return out.str();
}

WhereExpression make_comparison_expression(
    std::string column_name,
    ComparisonOperator comparison_operator,
    ValueNode value
) {
    WhereExpression expression;
    expression.kind = WhereExpressionKind::Comparison;
    expression.comparison = ComparisonExpression{
        std::move(column_name),
        comparison_operator,
        std::move(value)
    };

    return expression;
}

WhereExpression make_between_expression(
    std::string column_name,
    ValueNode lower_bound,
    ValueNode upper_bound
) {
    WhereExpression expression;
    expression.kind = WhereExpressionKind::Between;
    expression.between = BetweenExpression{
        std::move(column_name),
        std::move(lower_bound),
        std::move(upper_bound)
    };
    return expression;
}

WhereExpression make_logical_expression(
    LogicalOperator logical_operator,
    WhereExpression left,
    WhereExpression right
) {
    WhereExpression expression;
    expression.kind = WhereExpressionKind::Logical;
    expression.logical_operator = logical_operator;
    // Logical nodes own their children so WHERE can represent nested trees.
    expression.left = std::make_unique<WhereExpression>(std::move(left));
    expression.right = std::make_unique<WhereExpression>(std::move(right));
    return expression;
}

}  // namespace

Statement Parser::parse(const std::string& sql) const {
    Tokenizer tokenizer;
    const std::vector<Token> tokens = tokenizer.tokenize(sql);

    if (tokens.empty()) {
        throw SqlParseError("SQL input produced no tokens");
    }

    if (matches(tokens, 0, TokenType::Keyword, "CREATE")) {
        return parse_create_table(tokens);
    }

    if (matches(tokens, 0, TokenType::Keyword, "INSERT")) {
        return parse_insert(tokens);
    }

    if (matches(tokens, 0, TokenType::Keyword, "SELECT")) {
        return parse_select(tokens);
    }

    if (matches(tokens, 0, TokenType::Keyword, "DELETE")) {
        return parse_delete(tokens);
    }

    if (matches(tokens, 0, TokenType::Keyword, "UPDATE")) {
        return parse_update(tokens);
    }

    throw SqlParseError("Unsupported SQL statement");
}

const Token& Parser::expect(
    const std::vector<Token>& tokens,
    std::size_t index,
    TokenType expected_type,
    const std::string& expected_lexeme
) {
    if (index >= tokens.size()) {
        throw SqlParseError(make_parser_error("Unexpected end of token stream", index));
    }

    const Token& token = tokens[index];
    if (token.type != expected_type) {
        throw SqlParseError(make_parser_error("Unexpected token type", index));
    }

    if (!expected_lexeme.empty() && token.lexeme != expected_lexeme) {
        throw SqlParseError(make_parser_error("Unexpected token lexeme", index));
    }

    return token;
}

bool Parser::matches(
    const std::vector<Token>& tokens,
    std::size_t index,
    TokenType expected_type,
    const std::string& expected_lexeme
) {
    if (index >= tokens.size()) {
        return false;
    }

    const Token& token = tokens[index];
    if (token.type != expected_type) {
        return false;
    }

    if (!expected_lexeme.empty() && token.lexeme != expected_lexeme) {
        return false;
    }

    return true;
}

SqlTypeName Parser::parse_type_name(const Token& token, std::size_t token_index) {
    if (token.type != TokenType::TypeName) {
        throw SqlParseError(make_parser_error("Expected SQL type name token", token_index));
    }

    if (token.lexeme == "INTEGER") {
        return SqlTypeName::Integer;
    }

    if (token.lexeme == "STRING") {
        return SqlTypeName::String;
    }

    if (token.lexeme == "BOOLEAN") {
        return SqlTypeName::Boolean;
    }

    if (token.lexeme == "DATE") {
        return SqlTypeName::Date;
    }

    throw SqlParseError(make_parser_error("Unsupported SQL type name", token_index));
}

ValueNode Parser::parse_value(const std::vector<Token>& tokens, std::size_t& index) {
    if (index >= tokens.size()) {
        throw SqlParseError(make_parser_error("Unexpected end of token stream", index));
    }

    const Token& token = tokens[index];

    if (token.type == TokenType::Number) {
        ++index;
        return IntegerLiteral{std::stoll(token.lexeme)};
    }

    if (token.type == TokenType::String) {
        ++index;
        return StringLiteral{token.lexeme};
    }

    if (token.type == TokenType::Boolean) {
        ++index;
        return BooleanLiteral{token.lexeme == "TRUE"};
    }

    if (token.type == TokenType::TypeName && token.lexeme == "DATE") {
        ++index;
        const Token& date_value = expect(tokens, index++, TokenType::String);
        return DateLiteral{date_value.lexeme};
    }

    throw SqlParseError(make_parser_error("Expected SQL literal value", index));
}

WhereExpression Parser::parse_where_expression(
    const std::vector<Token>& tokens,
    std::size_t& index
) {
    // Start at the lowest-precedence operator so AND groups tighter than OR.
    return parse_or_expression(tokens, index);
}

WhereExpression Parser::parse_or_expression(
    const std::vector<Token>& tokens,
    std::size_t& index
) {
    WhereExpression left = parse_and_expression(tokens, index);

    // OR binds looser than AND, so each side is parsed as a full AND expression.
    while (matches(tokens, index, TokenType::Keyword, "OR")) {
        ++index;

        WhereExpression right = parse_and_expression(tokens, index);

        left = make_logical_expression(
            LogicalOperator::Or,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}

WhereExpression Parser::parse_and_expression(
    const std::vector<Token>& tokens,
    std::size_t& index
) {
    WhereExpression left = parse_predicate_expression(tokens, index);

    // AND combines predicate leaves before parse_or_expression sees any OR.
    while (matches(tokens, index, TokenType::Keyword, "AND")) {
        ++index;

        WhereExpression right = parse_predicate_expression(tokens, index);

        left = make_logical_expression(
            LogicalOperator::And,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}

WhereExpression Parser::parse_predicate_expression(
    const std::vector<Token>& tokens,
    std::size_t& index
) {
    const Token& column_name = expect(tokens, index++, TokenType::Identifier);

    // BETWEEN consumes its own AND, so it must be recognized before the
    // general logical-AND parser gets a chance to treat it as a tree join.
    if (matches(tokens, index, TokenType::Keyword, "BETWEEN")) {
        ++index;

        ValueNode lower_bound = parse_value(tokens, index);

        expect(tokens, index++, TokenType::Keyword, "AND");

        ValueNode upper_bound = parse_value(tokens, index);

        return make_between_expression(
            column_name.lexeme,
            std::move(lower_bound),
            std::move(upper_bound)
        );
    }

    const ComparisonOperator comparison_operator =
        parse_comparison_operator(tokens, index);

    ValueNode value = parse_value(tokens, index);

    return make_comparison_expression(
        column_name.lexeme,
        comparison_operator,
        std::move(value)
    );
}

ComparisonOperator Parser::parse_comparison_operator(
    const std::vector<Token>& tokens,
    std::size_t& index
) {
    if (matches(tokens, index, TokenType::Equals)) {
        ++index;
        return ComparisonOperator::Equal;
    }

    if (matches(tokens, index, TokenType::LessThan)) {
        ++index;
        return ComparisonOperator::LessThan;
    }

    if (matches(tokens, index, TokenType::LessThanOrEqual)) {
        ++index;
        return ComparisonOperator::LessThanOrEqual;
    }

    if (matches(tokens, index, TokenType::GreaterThan)) {
        ++index;
        return ComparisonOperator::GreaterThan;
    }

    if (matches(tokens, index, TokenType::GreaterThanOrEqual)) {
        ++index;
        return ComparisonOperator::GreaterThanOrEqual;
    }

    throw SqlParseError(make_parser_error("Expected comparison operator", index));
}

CreateTableStatement Parser::parse_create_table(const std::vector<Token>& tokens) {
    std::size_t index = 0;

    expect(tokens, index++, TokenType::Keyword, "CREATE");
    expect(tokens, index++, TokenType::Keyword, "TABLE");

    const Token& table_name = expect(tokens, index++, TokenType::Identifier);
    expect(tokens, index++, TokenType::LeftParen);

    std::vector<ColumnDefinitionNode> columns;

    while (true) {
        const Token& column_name = expect(tokens, index++, TokenType::Identifier);
        const std::size_t type_name_index = index;
        const Token& type_name = expect(tokens, index++, TokenType::TypeName);

        bool is_primary_key = false;
        bool is_auto_increment = false;

        while (
            matches(tokens, index, TokenType::Keyword, "PRIMARY") ||
            matches(tokens, index, TokenType::Keyword, "AUTOINCREMENT")
        ) {
            if (matches(tokens, index, TokenType::Keyword, "PRIMARY")) {
                ++index;
                expect(tokens, index++, TokenType::Keyword, "KEY");
                is_primary_key = true;
                continue;
            }

            if (matches(tokens, index, TokenType::Keyword, "AUTOINCREMENT")) {
                ++index;
                is_auto_increment = true;
                continue;
            }
        }

        columns.push_back(ColumnDefinitionNode{
            column_name.lexeme,
            parse_type_name(type_name, type_name_index),
            is_primary_key,
            is_auto_increment
        });

        if (matches(tokens, index, TokenType::Comma)) {
            ++index;
            continue;
        }

        break;
    }

    if (columns.empty()) {
        throw SqlParseError("CREATE TABLE must define at least one column");
    }

    expect(tokens, index++, TokenType::RightParen);
    expect(tokens, index++, TokenType::Semicolon);
    expect(tokens, index++, TokenType::EndOfInput);

    return CreateTableStatement{
        table_name.lexeme,
        std::move(columns)
    };
}

InsertStatement Parser::parse_insert(const std::vector<Token>& tokens) {
    std::size_t index = 0;

    expect(tokens, index++, TokenType::Keyword, "INSERT");
    expect(tokens, index++, TokenType::Keyword, "INTO");

    const Token& table_name = expect(tokens, index++, TokenType::Identifier);

    std::vector<std::string> column_names;

    if (matches(tokens, index, TokenType::LeftParen)) {
        ++index;

        if (matches(tokens, index, TokenType::RightParen)) {
            throw SqlParseError(make_parser_error("INSERT column list must define at least one column", index));
        }

        while (true) {
            const Token& column_name = expect(tokens, index++, TokenType::Identifier);
            column_names.push_back(column_name.lexeme);

            if (matches(tokens, index, TokenType::Comma)) {
                ++index;
                continue;
            }

            break;
        }

        expect(tokens, index++, TokenType::RightParen);
    }

    expect(tokens, index++, TokenType::Keyword, "VALUES");
    expect(tokens, index++, TokenType::LeftParen);

    std::vector<ValueNode> values;

    if (matches(tokens, index, TokenType::RightParen)) {
        throw SqlParseError(make_parser_error("INSERT must define at least one value", index));
    }

    while (true) {
        values.push_back(parse_value(tokens, index));

        if (matches(tokens, index, TokenType::Comma)) {
            ++index;
            continue;
        }

        break;
    }

    expect(tokens, index++, TokenType::RightParen);
    expect(tokens, index++, TokenType::Semicolon);
    expect(tokens, index++, TokenType::EndOfInput);

    return InsertStatement{
        table_name.lexeme,
        std::move(column_names),
        std::move(values)
    };
}

SelectStatement Parser::parse_select(const std::vector<Token>& tokens) {
    std::size_t index = 0;

    expect(tokens, index++, TokenType::Keyword, "SELECT");

    bool select_all = false;
    std::vector<std::string> column_names;

    if (matches(tokens, index, TokenType::Asterisk)) {
        select_all = true;
        ++index;
    } else {
        while (true) {
            const Token& column_name = expect(tokens, index++, TokenType::Identifier);
            column_names.push_back(column_name.lexeme);

            if (matches(tokens, index, TokenType::Comma)) {
                ++index;
                continue;
            }

            break;
        }
    }

    if (!select_all && column_names.empty()) {
        throw SqlParseError(make_parser_error("SELECT must define at least one column", index));
    }

    expect(tokens, index++, TokenType::Keyword, "FROM");

    const Token& table_name = expect(tokens, index++, TokenType::Identifier);

    std::optional<WhereExpression> where_expression;

    if (matches(tokens, index, TokenType::Keyword, "WHERE")) {
        ++index;
        where_expression = parse_where_expression(tokens, index);
    }

    expect(tokens, index++, TokenType::Semicolon);
    expect(tokens, index++, TokenType::EndOfInput);

    return SelectStatement{
        table_name.lexeme,
        select_all,
        std::move(column_names),
        std::move(where_expression)
    };
}

DeleteStatement Parser::parse_delete(const std::vector<Token>& tokens) {
    std::size_t index = 0;

    expect(tokens, index++, TokenType::Keyword, "DELETE");
    expect(tokens, index++, TokenType::Keyword, "FROM");

    const Token& table_name = expect(tokens, index++, TokenType::Identifier);

    std::optional<WhereExpression> where_expression;

    if (matches(tokens, index, TokenType::Keyword, "WHERE")) {
        ++index;
        where_expression = parse_where_expression(tokens, index);
    }

    expect(tokens, index++, TokenType::Semicolon);
    expect(tokens, index++, TokenType::EndOfInput);

    return DeleteStatement{
        table_name.lexeme,
        std::move(where_expression)
    };
}

UpdateStatement Parser::parse_update(const std::vector<Token>& tokens) {
    std::size_t index = 0;

    expect(tokens, index++, TokenType::Keyword, "UPDATE");

    const Token& table_name = expect(tokens, index++, TokenType::Identifier);

    expect(tokens, index++, TokenType::Keyword, "SET");

    std::vector<AssignmentExpression> assignments;

    while (true) {
        const Token& column_name = expect(tokens, index++, TokenType::Identifier);
        expect(tokens, index++, TokenType::Equals);

        assignments.push_back(AssignmentExpression{
            column_name.lexeme,
            parse_value(tokens, index)
        });

        if (matches(tokens, index, TokenType::Comma)) {
            ++index;
            continue;
        }

        break;
    }

    if (assignments.empty()) {
        throw SqlParseError(make_parser_error("UPDATE must define at least one assignment", index));
    }

    std::optional<WhereExpression> where_expression;

    if (matches(tokens, index, TokenType::Keyword, "WHERE")) {
        ++index;
        where_expression = parse_where_expression(tokens, index);
    }

    expect(tokens, index++, TokenType::Semicolon);
    expect(tokens, index++, TokenType::EndOfInput);

    return UpdateStatement{
        table_name.lexeme,
        std::move(assignments),
        std::move(where_expression)
    };
}

}  // namespace db::sql
