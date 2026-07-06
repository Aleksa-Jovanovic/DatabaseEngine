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

For `CREATE TABLE`, the flow is slightly different:
```text
SQL text
-> tokenizer/parser
-> CreateTableStatement
-> Executor
-> Catalog create_table(...)
-> eager table/index file bootstrap
```

For `DROP TABLE`, execution goes through the catalog as well:
```text
SQL text
-> tokenizer/parser
-> DropTableStatement
-> Executor
-> Catalog drop_table(...)
-> catalog metadata removal
-> heap/index file removal
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
- affected row count for write statements

## Current CREATE TABLE behavior
The current executor supports a first SQL-driven table creation path.

Supported form:
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name STRING,
    active BOOLEAN
);
```

Current behavior:
- builds a `catalog::TableDefinition` from the parsed SQL AST
- maps SQL types into catalog column types
- requires exactly one `PRIMARY KEY` column through catalog validation
- supports primary-key columns that are not the first column
- creates a primary index definition for the primary-key column
- uses generated file names like `users_heap.db` and
  `users_primary_index.db`
- delegates validation and physical bootstrap to `Catalog::create_table(...)`

Current limitation:
- table-level constraints are not supported yet
- file names are generated from the table name rather than chosen by SQL
- secondary indexes are not created from SQL yet
- `AUTOINCREMENT` is only meaningful for the integer primary-key column

## Current DROP TABLE behavior
The current executor supports a first SQL-driven table drop path.

Supported form:
```sql
DROP TABLE users;
```

Current behavior:
- delegates to `Catalog::drop_table(...)`
- removes the table definition from catalog metadata
- removes the heap file
- removes the primary and secondary index files listed in catalog metadata
- returns `affected_rows = 0` because this is DDL, not row-level DML

Current limitation:
- drop is not crash-safe or transactional
- if physical file removal succeeds but catalog persistence fails, the current
  implementation restores in-memory metadata but does not recreate deleted
  files

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

## Current INSERT behavior
The current executor supports table-level `INSERT` execution.

Supported forms:
- schema-order inserts with all values
- schema-order inserts that omit an auto-increment primary key
- named-column inserts
- named-column inserts with reordered columns
- named-column inserts that omit non-primary columns
- named-column inserts that omit an auto-increment primary key

When values are omitted, the executor fills missing non-primary columns with
simple type defaults:
- integer columns default to `0`
- string columns default to `""`
- boolean columns default to `false`
- date columns default to an empty `DateValue`

When the primary key is omitted, the executor first checks whether the
primary-key column is marked `AUTOINCREMENT`. If it is, the executor asks the
opened `Table` for a generated primary key and writes that generated key into
the row before insert. If it is not, the insert fails because the caller must
provide the primary-key value explicitly.

The current table auto-increment counter is live runtime state. It is rebuilt
by scanning existing rows when a table object opens, and it advances during
successful inserts. The counter is not persisted in catalog metadata yet.

## Current UPDATE behavior
The current executor supports table-level `UPDATE` execution through scan-based
row matching.

Supported forms:
- update rows matched by `WHERE`
- update all rows when `WHERE` is omitted
- update one or more non-primary-key columns
- return the number of affected rows in `ExecutionResult::affected_rows`

The executor applies assignments to scanned rows and then calls
`Table::update_by_key(...)` for each matching row.

Current limitation:
- updating the primary key is rejected

Changing a primary key requires coordinated primary-index maintenance and extra
duplicate-key handling. That is intentionally deferred even though the first
B+ tree delete path now exists.

## Current DELETE behavior
The current executor supports table-level `DELETE` execution through scan-based
row matching.

Supported forms:
- delete rows matched by `WHERE`
- delete all rows when `WHERE` is omitted
- return the number of affected rows in `ExecutionResult::affected_rows`

The executor scans the table, evaluates the optional `WHERE` expression against
each full row, and calls `Table::delete_by_key(...)` for every matching row.

Current limitation:
- delete execution is scan-based and does not use index scans yet
- table-level delete currently maintains the primary index only
- the B+ tree delete path removes leaf entries but does not rebalance yet

## Current limitations
- filtering is in-memory after `Table::scan()`
- no secondary-index lookup during execution yet
- auto-increment state is rebuilt from table rows on open instead of persisted
  as catalog/table metadata
- primary-key-changing `UPDATE` is not supported yet
- query result rows still use `table::Row`, so they carry `Row::key` even when
  the primary-key column is not projected
- no planner or physical operator tree yet

## Current test coverage
The current executor create-table test verifies:
- SQL-driven `CREATE TABLE`
- primary-key and auto-increment metadata propagation into catalog definitions
- generated heap and primary-index file names
- omitted primary-key insert succeeds for auto-increment tables
- duplicate table creation failure
- non-first primary-key columns
- omitted primary-key insert failure when the primary key is not auto-increment
- non-integer primary-key rejection

The current executor drop-table test verifies:
- SQL-driven `DROP TABLE`
- catalog metadata removal
- heap and primary-index file removal
- failed `SELECT` after drop
- repeated drop failure

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

The current executor insert test verifies:
- full schema-order inserts
- named-column inserts
- reordered named-column inserts
- omitted non-primary columns with defaults
- omitted primary key with live auto-increment
- missing manual primary-key failure when the primary key is not auto-increment
- manual high primary keys advancing the next generated key
- duplicate primary-key insert failure
- missing-table insert failure
- invalid primary-key type failure
- duplicate named-column failure

The current executor update test verifies:
- updating one row by primary-key predicate
- updating one row by non-primary predicate
- updating all rows when `WHERE` is omitted
- zero affected rows when no rows match
- primary-key-changing update failure
- missing-table update failure
- unknown assignment-column failure
- invalid assignment type failure

The current executor delete test verifies:
- deleting one row by primary-key predicate
- deleting rows by non-primary predicate
- deleting all rows when `WHERE` is omitted
- zero affected rows when no rows match
- missing-table delete failure

## Next steps
Good next execution milestones:
- introduce a query-result row type that is separate from storage/table rows
- add basic planner structure
- add index-scan execution for primary-key predicates
- add SQL support for secondary-index creation
