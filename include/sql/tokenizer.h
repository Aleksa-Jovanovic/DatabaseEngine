#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace db::sql
{

class SqlParseError : public std::runtime_error {
public:
    explicit SqlParseError(const std::string& message)
        : std::runtime_error(message) {}
};

enum class TokenType
{
    Keyword,
    Identifier,
    TypeName,
    Number,
    String,
    Boolean,
    Comma,
    LeftParen,
    RightParen,
    Semicolon,
    Asterisk,
    Equals,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
    EndOfInput
};

struct Token
{
    TokenType type;
    std::string lexeme;
};

class Tokenizer
{
public:
    std::vector<Token> tokenize(const std::string &sql) const;

private:
    static bool is_keyword(const std::string &token);
    static bool is_type_name(const std::string &token);
    static bool is_boolean_literal(const std::string& token);
    static std::string normalize_identifier(const std::string &token);
};

} // namespace db::sql