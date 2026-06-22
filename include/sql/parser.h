#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "sql/ast.h"
#include "sql/tokenizer.h"

namespace db::sql {

class Parser {
public:
    Statement parse(const std::string& sql) const;

private:
    static const Token& expect(
        const std::vector<Token>& tokens,
        std::size_t index,
        TokenType expected_type,
        const std::string& expected_lexeme = ""
    );

    static bool matches(
        const std::vector<Token>& tokens,
        std::size_t index,
        TokenType expected_type,
        const std::string& expected_lexeme = ""
    );

    static SqlTypeName parse_type_name(const Token& token, std::size_t token_index);
    static ValueNode parse_value(const std::vector<Token>& tokens, std::size_t& index);

    // WHERE parsing is split by precedence: OR is lowest, AND is higher,
    // and predicates are the comparison/BETWEEN leaves of the expression tree.
    static WhereExpression parse_where_expression(
        const std::vector<Token>& tokens,
        std::size_t& index
    );
    static WhereExpression parse_or_expression(
        const std::vector<Token>& tokens,
        std::size_t& index
    );
    static WhereExpression parse_and_expression(
        const std::vector<Token>& tokens,
        std::size_t& index
    );
    static WhereExpression parse_predicate_expression(
        const std::vector<Token>& tokens,
        std::size_t& index
    );
    static ComparisonOperator parse_comparison_operator(
        const std::vector<Token>& tokens,
        std::size_t& index
    );

    static CreateTableStatement parse_create_table(const std::vector<Token>& tokens);
    static InsertStatement parse_insert(const std::vector<Token>& tokens);
    static SelectStatement parse_select(const std::vector<Token>& tokens);
    static DeleteStatement parse_delete(const std::vector<Token>& tokens);
    static UpdateStatement parse_update(const std::vector<Token>& tokens);
};

}  // namespace db::sql
