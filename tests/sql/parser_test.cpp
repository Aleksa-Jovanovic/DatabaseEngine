#include <cassert>
#include <iostream>
#include <string>
#include <variant>

#include "sql/ast.h"
#include "sql/parser.h"
#include "sql/tokenizer.h"

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
            parser.parse("SELECT * FROM users;");
        } catch (const db::sql::SqlParseError& ex) {
            threw = true;
            assert(std::string(ex.what()).find("Unsupported SQL statement") != std::string::npos);
        }

        assert(threw);
    }

    std::cout << "Parser test passed.\n";
    return 0;
}