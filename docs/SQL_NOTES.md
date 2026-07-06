# SQL Notes

## Current SQL goal
Phase 8 introduces the first SQL-facing layer of the project.

The immediate goal is to move from direct method calls into a pipeline where:
- SQL text is tokenized
- parsed into an AST
- later executed against the catalog, tables, and indexes

## Current tokenizer model
The current tokenizer lives in `sql::Tokenizer`.

It now produces explicit tokens instead of plain strings.

Current token model:
- `TokenType`
  - `Keyword`
  - `Identifier`
  - `TypeName`
  - `Number`
  - `String`
  - `Boolean`
  - `Comma`
  - `LeftParen`
  - `RightParen`
  - `Semicolon`
  - `Asterisk`
  - `Equals`
  - `LessThan`
  - `LessThanOrEqual`
  - `GreaterThan`
  - `GreaterThanOrEqual`
  - `EndOfInput`
- `Token`
  - token type
  - lexeme

This gives later parser code a more stable input representation than raw
string splitting.

## Current tokenization behavior
The tokenizer currently:
- skips whitespace
- recognizes punctuation tokens directly
- reads integer numeric literals
- reads quoted string literals
- normalizes keyword and type-name matching case-insensitively
- preserves identifier lexemes as written
- appends an explicit `EndOfInput` token

Current SQL keywords:
- `CREATE`
- `DROP`
- `PRIMARY`
- `KEY`
- `AUTOINCREMENT`
- `TABLE`
- `INSERT`
- `INTO`
- `VALUES`
- `SELECT`
- `FROM`
- `WHERE`
- `DELETE`
- `UPDATE`
- `SET`
- `AND`
- `OR`
- `BETWEEN`

Current SQL type names:
- `INTEGER`
- `STRING`
- `BOOLEAN`
- `DATE`

## Current SQL error handling
The SQL layer now has a first dedicated parse-time exception type:
- `SqlParseError`

Current tokenizer errors include:
- unterminated string literals
- unexpected characters in SQL input

Current parser errors include:
- unexpected token type
- unexpected token lexeme
- unexpected end of token stream
- unsupported SQL statement
- unsupported SQL type name
- invalid empty `INSERT` column lists
- invalid empty `INSERT` value lists
- invalid or missing `WHERE` comparison operators
- malformed `DELETE` statements
- malformed `UPDATE` assignments

Error messages now include character positions in the tokenizer and token
indexes in the parser to make malformed input easier to debug.

## Current AST and parser model
The SQL layer now has a first typed AST for `CREATE TABLE`, `DROP TABLE`,
`INSERT`, `SELECT`, `DELETE`, and `UPDATE`.

Current AST pieces:
- `SqlTypeName`
  - `Integer`
  - `String`
  - `Boolean`
  - `Date`
- `ColumnDefinitionNode`
  - column name
  - SQL type name
  - whether the column is declared `PRIMARY KEY`
  - whether the column is declared `AUTOINCREMENT`
- `CreateTableStatement`
  - table name
  - column definitions
- `DropTableStatement`
  - table name
- literal value nodes
  - integer literals
  - string literals
  - boolean literals
  - date literals
- `InsertStatement`
  - table name
  - optional column-name list
  - value list
- `ComparisonOperator`
  - `Equal`
  - `LessThan`
  - `LessThanOrEqual`
  - `GreaterThan`
  - `GreaterThanOrEqual`
- `LogicalOperator`
  - `And`
  - `Or`
- `WhereExpression`
  - comparison predicates
  - `BETWEEN` predicates
  - logical expression-tree nodes
- `ComparisonExpression`
  - column name
  - comparison operator
  - literal value
- `BetweenExpression`
  - column name
  - lower-bound literal value
  - upper-bound literal value
- `SelectStatement`
  - table name
  - either `*` or explicit projected columns
  - optional `WHERE` expression
- `DeleteStatement`
  - table name
  - optional `WHERE` expression
- `AssignmentExpression`
  - column name
  - literal value
- `UpdateStatement`
  - table name
  - assignment list
  - optional `WHERE` expression
- `Statement`
  - currently a variant over `CREATE TABLE`, `DROP TABLE`, `INSERT`,
    `SELECT`, `DELETE`, and `UPDATE` statement nodes

Current parser behavior:
- tokenizes the SQL input
- recognizes `CREATE TABLE` statements
- parses column definitions with SQL type names
- parses optional `PRIMARY KEY` and `AUTOINCREMENT` column constraints
- builds a typed `CreateTableStatement` AST node
- recognizes `DROP TABLE ...` statements
- builds a typed `DropTableStatement` AST node
- recognizes `INSERT INTO ... VALUES (...)` statements
- recognizes `INSERT INTO ... (column, column) VALUES (...)` statements
- parses integer, string, boolean, and date literal values
- recognizes `SELECT * FROM ...` statements
- recognizes `SELECT column, column FROM ...` statements
- recognizes `DELETE FROM ...` statements
- recognizes `UPDATE ... SET ...` statements
- parses optional `WHERE` expression trees
- supports `=`, `<`, `<=`, `>`, and `>=` comparison operators
- supports `AND`, `OR`, and `BETWEEN` in `WHERE`
- preserves SQL precedence by binding `AND` tighter than `OR`
- leaves semantic checks, such as column/value count matching, to the later
  execution layer where table schema metadata is available

## Current testing
The current SQL tokenizer test verifies:
- tokenization of `CREATE TABLE` with `PRIMARY KEY AUTOINCREMENT`
- tokenization of a basic `DROP TABLE` statement
- tokenization of a basic `INSERT` statement
- tokenization of a basic `SELECT` statement
- tokenization of basic `DELETE` and `UPDATE` statements
- recognition of `BOOLEAN` and `DATE` as `TypeName`
- recognition of comparison operators used by `WHERE`
- presence of the final `EndOfInput` token
- throwing `SqlParseError` for an unterminated string literal

The current SQL parser test verifies:
- parsing `CREATE TABLE` statements into AST form
- parsing `PRIMARY KEY` and `AUTOINCREMENT` column flags
- parsing primary-key columns that are not the first column
- parsing `DROP TABLE` statements into AST form
- parsing `BOOLEAN` and `DATE` column types into `SqlTypeName`
- parsing positional `INSERT` values into AST form
- parsing named-column `INSERT` values into AST form
- parsing integer, string, boolean, and date literal values
- parsing `SELECT * FROM ...` into AST form
- parsing projected-column `SELECT` statements into AST form
- parsing optional `WHERE` filters into AST form
- parsing `=`, `<`, `<=`, `>`, and `>=` comparison operators
- parsing `AND`, `OR`, and `BETWEEN` into `WHERE` expression trees
- preserving `AND` precedence over `OR`
- parsing `DELETE` statements with and without `WHERE`
- parsing `UPDATE` statements with single and multiple assignments
- parsing `UPDATE` statements with and without `WHERE`
- rejection of missing semicolons
- rejection of missing type names
- rejection of empty `INSERT` column lists
- rejection of empty `INSERT` value lists
- rejection of malformed `SELECT` and `WHERE` statements
- rejection of malformed `DROP TABLE`, `DELETE`, and `UPDATE` statements

## Current limitations
- tokenizer only handles a small SQL subset
- parser currently handles basic `CREATE TABLE`, `INSERT`, `SELECT`,
  `DELETE`, `UPDATE`, and `DROP TABLE`
- AST currently models the basic CRUD SQL surface needed by the next phase
- `CREATE TABLE` supports inline column-level `PRIMARY KEY` and
  `AUTOINCREMENT`, but not table-level constraints yet
- numeric literals are currently integer-only
- string literals do not yet support escaping
- date literals are currently represented as strings after `DATE '...'`
- parser does not yet validate inserted values against table schemas
- parser does not yet validate selected columns against table schemas
- parser does not yet validate updated columns against table schemas
- parenthesized boolean expressions are not supported yet
- joins, ordering, grouping, aliases, and aggregates are not supported yet
- SQL execution exists for the current Phase 9 subset, but optimization and
  richer DDL are still limited

## Current phase boundary
Phase 8 is complete for the current parser/tokenizer/AST milestone.

The SQL layer now has:
- a typed tokenizer
- dedicated SQL parse errors
- a typed AST for the current CRUD SQL subset
- parser support for `CREATE TABLE`
- parser support for `DROP TABLE`
- parser support for `INSERT`
- parser support for `SELECT`
- parser support for `DELETE`
- parser support for `UPDATE`
- parser support for comparison, `BETWEEN`, `AND`, and `OR` in `WHERE`
- separate tokenizer and parser tests

The next natural parser-side steps are:
- later extend SQL syntax with parenthesized boolean expressions if needed
- later add SQL syntax for secondary index creation
- later add richer DDL such as `ALTER TABLE`
