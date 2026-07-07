// SQL tokenizer implementation.
#include "sql/tokenizer.h"

#include <cctype>
#include <sstream>
#include <unordered_set>

namespace db::sql
{

namespace
{

const std::unordered_set<std::string> kKeywords = {
    "CREATE",
    "DROP",
    "INDEX",
    "ON",
    "PRIMARY",
    "KEY",
    "AUTOINCREMENT",
    "TABLE",
    "INSERT",
    "INTO",
    "VALUES",
    "SELECT",
    "FROM",
    "WHERE",
    "DELETE",
    "UPDATE",
    "SET",
    "AND",
    "OR",
    "BETWEEN"
};

const std::unordered_set<std::string> kTypeNames = {
    "INTEGER",
    "STRING",
    "BOOLEAN",
    "DATE"
};

const std::unordered_set<std::string> kBooleanLiterals = {
    "TRUE",
    "FALSE"
};

std::string make_error_with_position(const std::string& message, std::size_t position) {
    std::ostringstream out;
    out << message << " at position " << position;
    return out.str();
}

bool is_identifier_start(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_identifier_part(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

std::string to_upper_ascii(const std::string &value)
{
    std::string result = value;
    for (char &ch : result)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return result;
}

} // namespace

std::vector<Token> Tokenizer::tokenize(const std::string &sql) const
{
    std::vector<Token> tokens;
    std::size_t i = 0;

    while (i < sql.size())
    {
        const char ch = sql[i];

        if (std::isspace(static_cast<unsigned char>(ch)))
        {
            ++i;
            continue;
        }

        switch (ch)
        {
        case ',':
            tokens.push_back({TokenType::Comma, ","});
            ++i;
            continue;
        case '(':
            tokens.push_back({TokenType::LeftParen, "("});
            ++i;
            continue;
        case ')':
            tokens.push_back({TokenType::RightParen, ")"});
            ++i;
            continue;
        case ';':
            tokens.push_back({TokenType::Semicolon, ";"});
            ++i;
            continue;
        case '*':
            tokens.push_back({TokenType::Asterisk, "*"});
            ++i;
            continue;
        case '=':
            tokens.push_back({TokenType::Equals, "="});
            ++i;
            continue;
        case '<':
            if (i + 1 < sql.size() && sql[i + 1] == '=') {
                tokens.push_back({TokenType::LessThanOrEqual, "<="});
                i += 2;
            } else {
                tokens.push_back({TokenType::LessThan, "<"});
                ++i;
            }
            continue;

        case '>':
            if (i + 1 < sql.size() && sql[i + 1] == '=') {
                tokens.push_back({TokenType::GreaterThanOrEqual, ">="});
                i += 2;
            } else {
                tokens.push_back({TokenType::GreaterThan, ">"});
                ++i;
            }
            continue;
        case '\'':
        {
            ++i;
            const std::size_t start = i;

            while (i < sql.size() && sql[i] != '\'')
            {
                ++i;
            }

            if (i >= sql.size())
            {
                throw SqlParseError(
                    make_error_with_position("Unterminated SQL string literal", start - 1)
                );
            }

            tokens.push_back({TokenType::String, sql.substr(start, i - start)});
            ++i;
            continue;
        }
        default:
            break;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)))
        {
            const std::size_t start = i;
            while (i < sql.size() && std::isdigit(static_cast<unsigned char>(sql[i])))
            {
                ++i;
            }

            tokens.push_back({TokenType::Number, sql.substr(start, i - start)});
            continue;
        }

        if (is_identifier_start(ch))
        {
            const std::size_t start = i;
            ++i;

            while (i < sql.size() && is_identifier_part(sql[i]))
            {
                ++i;
            }

            const std::string raw = sql.substr(start, i - start);
            const std::string normalized = normalize_identifier(raw);

            if (is_keyword(normalized)) {
                tokens.push_back({TokenType::Keyword, normalized});
            } else if (is_type_name(normalized)) {
                tokens.push_back({TokenType::TypeName, normalized});
            } else if (is_boolean_literal(normalized)) {
                tokens.push_back({TokenType::Boolean, normalized});
            } else {
                tokens.push_back({TokenType::Identifier, raw});
            }

            continue;
        }

        {
            std::ostringstream out;
            out << "Unexpected character '" << ch << "' in SQL input at position " << i;
            throw SqlParseError(out.str());
        }
    }

    tokens.push_back({TokenType::EndOfInput, ""});
    return tokens;
}

bool Tokenizer::is_keyword(const std::string &token)
{
    return kKeywords.find(token) != kKeywords.end();
}

bool Tokenizer::is_type_name(const std::string& token) {
    return kTypeNames.find(token) != kTypeNames.end();
}

bool Tokenizer::is_boolean_literal(const std::string& token)
{
    return kBooleanLiterals.find(token) != kBooleanLiterals.end();
}

std::string Tokenizer::normalize_identifier(const std::string &token)
{
    return to_upper_ascii(token);
}

} // namespace db::sql
