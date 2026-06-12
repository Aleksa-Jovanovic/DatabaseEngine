#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "sql/tokenizer.h"

namespace {

using db::sql::SqlParseError;
using db::sql::Token;
using db::sql::TokenType;
using db::sql::Tokenizer;

void assert_token(const Token& token, TokenType expected_type, const std::string& expected_lexeme) {
    assert(token.type == expected_type);
    assert(token.lexeme == expected_lexeme);
}

}  // namespace

int main() {
    Tokenizer tokenizer;

    {
        const std::vector<Token> tokens =
            tokenizer.tokenize("CREATE TABLE users (id INTEGER, name STRING);");

        assert(tokens.size() == 12);
        assert_token(tokens[0], TokenType::Keyword, "CREATE");
        assert_token(tokens[1], TokenType::Keyword, "TABLE");
        assert_token(tokens[2], TokenType::Identifier, "users");
        assert_token(tokens[3], TokenType::LeftParen, "(");
        assert_token(tokens[4], TokenType::Identifier, "id");
        assert_token(tokens[5], TokenType::TypeName, "INTEGER");
        assert_token(tokens[6], TokenType::Comma, ",");
        assert_token(tokens[7], TokenType::Identifier, "name");
        assert_token(tokens[8], TokenType::TypeName, "STRING");
        assert_token(tokens[9], TokenType::RightParen, ")");
        assert_token(tokens[10], TokenType::Semicolon, ";");
        assert_token(tokens[11], TokenType::EndOfInput, "");
    }

    {
        const std::vector<Token> tokens =
            tokenizer.tokenize("INSERT INTO users VALUES (1, 'Alice');");

        assert(tokens.size() == 11);
        assert_token(tokens[0], TokenType::Keyword, "INSERT");
        assert_token(tokens[1], TokenType::Keyword, "INTO");
        assert_token(tokens[2], TokenType::Identifier, "users");
        assert_token(tokens[3], TokenType::Keyword, "VALUES");
        assert_token(tokens[4], TokenType::LeftParen, "(");
        assert_token(tokens[5], TokenType::Number, "1");
        assert_token(tokens[6], TokenType::Comma, ",");
        assert_token(tokens[7], TokenType::String, "Alice");
        assert_token(tokens[8], TokenType::RightParen, ")");
        assert_token(tokens[9], TokenType::Semicolon, ";");
        assert_token(tokens[10], TokenType::EndOfInput, "");
    }

    {
        const std::vector<Token> tokens =
            tokenizer.tokenize("SELECT * FROM users WHERE id = 42;");

        assert(tokens.size() == 10);
        assert_token(tokens[0], TokenType::Keyword, "SELECT");
        assert_token(tokens[1], TokenType::Asterisk, "*");
        assert_token(tokens[2], TokenType::Keyword, "FROM");
        assert_token(tokens[3], TokenType::Identifier, "users");
        assert_token(tokens[4], TokenType::Keyword, "WHERE");
        assert_token(tokens[5], TokenType::Identifier, "id");
        assert_token(tokens[6], TokenType::Equals, "=");
        assert_token(tokens[7], TokenType::Number, "42");
        assert_token(tokens[8], TokenType::Semicolon, ";");
        assert_token(tokens[9], TokenType::EndOfInput, "");
    }

    {
        const std::vector<Token> tokens =
            tokenizer.tokenize("CREATE TABLE flags (enabled BOOLEAN, created_at DATE);");

        assert(tokens.size() == 12);
        assert_token(tokens[0], TokenType::Keyword, "CREATE");
        assert_token(tokens[1], TokenType::Keyword, "TABLE");
        assert_token(tokens[2], TokenType::Identifier, "flags");
        assert_token(tokens[3], TokenType::LeftParen, "(");
        assert_token(tokens[4], TokenType::Identifier, "enabled");
        assert_token(tokens[5], TokenType::TypeName, "BOOLEAN");
        assert_token(tokens[6], TokenType::Comma, ",");
        assert_token(tokens[7], TokenType::Identifier, "created_at");
        assert_token(tokens[8], TokenType::TypeName, "DATE");
        assert_token(tokens[9], TokenType::RightParen, ")");
        assert_token(tokens[10], TokenType::Semicolon, ";");
        assert_token(tokens[11], TokenType::EndOfInput, "");
    }

    {
        bool threw = false;
        try {
            tokenizer.tokenize("INSERT INTO users VALUES ('Alice);");
        } catch (const SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Unterminated SQL string literal") != std::string::npos);
        }

        assert(threw);
    }

    std::cout << "Tokenizer test passed.\n";
    return 0;
}