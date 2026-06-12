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

Error messages now include character positions in the tokenizer and token
indexes in the parser to make malformed input easier to debug.

## Current AST and parser model
The SQL layer now has a first typed AST for `CREATE TABLE`.

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
- `Statement`
  - currently a `std::variant<CreateTableStatement>`

Current parser behavior:
- tokenizes the SQL input
- recognizes `CREATE TABLE` statements
- parses column definitions with SQL type names
- builds a typed `CreateTableStatement` AST node

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
- rejection of missing semicolons
- rejection of missing type names
- rejection of unsupported top-level statements such as `SELECT`

## Current limitations
- tokenizer only handles a small SQL subset
- parser currently only handles `CREATE TABLE`
- AST currently only models `CREATE TABLE`
- numeric literals are currently integer-only
- string literals do not yet support escaping
- there is no SQL execution path yet

## Current phase boundary
This is the first structural slice of Phase 8.

The SQL layer now has:
- a typed tokenizer
- dedicated SQL parse errors
- a first typed AST
- a first parser for `CREATE TABLE`
- separate tokenizer and parser tests

The next natural steps are:
- connect parsed `CREATE TABLE` statements to catalog operations
- extend parsing to `INSERT`
- then extend parsing to `SELECT`
