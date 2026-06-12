#include "sql/parser.h"

#include <sstream>

namespace db::sql {
namespace {

std::string make_parser_error(const std::string& message, std::size_t token_index) {
    std::ostringstream out;
    out << message << " at token index " << token_index;
    return out.str();
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

        columns.push_back(ColumnDefinitionNode{
            column_name.lexeme,
            parse_type_name(type_name, type_name_index)
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

}  // namespace db::sql
