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
- `TABLE`
- `INSERT`
- `INTO`
- `VALUES`
- `SELECT`
- `FROM`
- `WHERE`

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

Error messages now include character positions in the tokenizer and token
indexes in the parser to make malformed input easier to debug.

## Current AST and parser model
The SQL layer now has a first typed AST for `CREATE TABLE` and `INSERT`.

Current AST pieces:
- `SqlTypeName`
  - `Integer`
  - `String`
  - `Boolean`
  - `Date`
- `ColumnDefinitionNode`
  - column name
  - SQL type name
- `CreateTableStatement`
  - table name
  - column definitions
- literal value nodes
  - integer literals
  - string literals
  - boolean literals
  - date literals
- `InsertStatement`
  - table name
  - optional column-name list
  - value list
- `Statement`
  - currently a `std::variant<CreateTableStatement, InsertStatement>`

Current parser behavior:
- tokenizes the SQL input
- recognizes `CREATE TABLE` statements
- parses column definitions with SQL type names
- builds a typed `CreateTableStatement` AST node
- recognizes `INSERT INTO ... VALUES (...)` statements
- recognizes `INSERT INTO ... (column, column) VALUES (...)` statements
- parses integer, string, boolean, and date literal values
- leaves semantic checks, such as column/value count matching, to the later
  execution layer where table schema metadata is available

## Current testing
The current SQL tokenizer test verifies:
- tokenization of a basic `CREATE TABLE` statement
- tokenization of a basic `INSERT` statement
- tokenization of a basic `SELECT` statement
- recognition of `BOOLEAN` and `DATE` as `TypeName`
- presence of the final `EndOfInput` token
- throwing `SqlParseError` for an unterminated string literal

The current SQL parser test verifies:
- parsing a basic `CREATE TABLE` statement into AST form
- parsing `BOOLEAN` and `DATE` column types into `SqlTypeName`
- parsing positional `INSERT` values into AST form
- parsing named-column `INSERT` values into AST form
- parsing integer, string, boolean, and date literal values
- rejection of missing semicolons
- rejection of missing type names
- rejection of empty `INSERT` column lists
- rejection of empty `INSERT` value lists
- rejection of unsupported top-level statements such as `SELECT`

## Current limitations
- tokenizer only handles a small SQL subset
- parser currently handles `CREATE TABLE` and basic `INSERT`
- AST currently models `CREATE TABLE` and basic `INSERT`
- numeric literals are currently integer-only
- string literals do not yet support escaping
- date literals are currently represented as strings after `DATE '...'`
- parser does not yet validate inserted values against table schemas
- there is no SQL execution path yet

## Current phase boundary
This is the first structural slice of Phase 8.

The SQL layer now has:
- a typed tokenizer
- dedicated SQL parse errors
- a first typed AST
- a first parser for `CREATE TABLE`
- parser support for basic `INSERT`
- separate tokenizer and parser tests

The next natural steps are:
- connect parsed `CREATE TABLE` statements to catalog operations
- connect parsed `INSERT` statements to table insert operations
- then extend parsing to `SELECT`
