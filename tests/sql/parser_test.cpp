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
