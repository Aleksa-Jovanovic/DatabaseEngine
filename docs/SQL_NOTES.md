# SQL Notes

## Current SQL goal
Phase 8 introduces the first SQL-facing layer of the project.

The immediate goal is to move from direct method calls into a pipeline where:
- SQL text is tokenized
- later parsed into an AST
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

Error messages now include the character position to make malformed input
easier to debug.

## Current testing
The current SQL tokenizer test verifies:
- tokenization of a basic `CREATE TABLE` statement
- tokenization of a basic `INSERT` statement
- tokenization of a basic `SELECT` statement
- recognition of `BOOLEAN` and `DATE` as `TypeName`
- presence of the final `EndOfInput` token
- throwing `SqlParseError` for an unterminated string literal

## Current limitations
- parser behavior is still only a skeleton
- AST structures are still placeholders
- tokenizer only handles a small SQL subset
- numeric literals are currently integer-only
- string literals do not yet support escaping
- there is no SQL execution path yet

## Current phase boundary
This is the first structural slice of Phase 8.

The SQL layer now has:
- a typed tokenizer
- dedicated SQL parse errors
- initial tests for the first supported statement forms

The next natural steps are:
- define AST structures
- parse `CREATE TABLE`
- then extend parsing to `INSERT` and `SELECT`
