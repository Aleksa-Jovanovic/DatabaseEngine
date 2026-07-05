# Execution Notes

## Current execution goal
Phase 9 introduces the first query execution layer on top of the SQL parser,
catalog, table abstraction, heap storage, and indexes.

The purpose of this layer is to turn parsed SQL AST nodes into real table
operations.

Current flow:
```text
SQL text
-> tokenizer/parser
-> SQL AST
-> Executor
-> Catalog lookup
-> Table open
-> table scan
-> filtering/projection
-> ExecutionResult
```

## Current executor design
The current `Executor` lives in the execution layer rather than the SQL layer.

This keeps responsibilities separated:
- SQL layer parses text and builds AST nodes
- catalog layer knows table definitions and schemas
- table layer reads and writes rows
- execution layer coordinates parsed statements against catalog/table APIs

The current `ExecutionResult` contains:
- success flag
- error message
- result column names
- result rows

## Current SELECT behavior
The current executor supports scan-based `SELECT`.

Supported forms:
- `SELECT * FROM table`
- `SELECT column_a, column_b FROM table`
- `WHERE` comparison predicates
- `WHERE BETWEEN`
- `WHERE AND`
- `WHERE OR`

The executor currently evaluates `WHERE` before projection:
```text
scan full rows
-> filter using full table schema
-> project selected columns
```

This is important for queries such as:
```sql
SELECT id FROM users WHERE name = 'Alice';
```

The `name` column is still available during filtering even though the final
result only includes `id`.

## Current WHERE behavior
Supported comparison behavior:
- integer comparisons
- string comparisons
- boolean equality
- date comparisons through the `DateValue` string representation

Supported logical behavior:
- `AND`
- `OR`
- SQL-style precedence from the parser, where `AND` binds tighter than `OR`

Current filtering is scan-based. No index scan is used yet.

## Current limitations
- only `SELECT` execution is implemented
- `INSERT`, `UPDATE`, `DELETE`, and `CREATE TABLE` execution are not wired yet
- filtering is in-memory after `Table::scan()`
- no secondary-index lookup during execution yet
- query result rows still use `table::Row`, so they carry `Row::key` even when
  the primary-key column is not projected
- no planner or physical operator tree yet

## Current test coverage
The current executor test verifies:
- executing `SELECT *`
- executing projected `SELECT`
- missing-table failure
- unknown projected-column failure
- comparison-based `WHERE`
- `BETWEEN`
- `AND`
- `OR`
- logical precedence between `AND` and `OR`

## Next steps
Good next execution milestones:
- execute `INSERT`
- introduce a query-result row type that is separate from storage/table rows
- add basic planner structure
- add index-scan execution for primary-key predicates
- execute `UPDATE` and `DELETE`
