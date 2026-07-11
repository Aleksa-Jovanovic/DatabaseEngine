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

Index-management statements also delegate to the catalog:
```text
SQL text
-> tokenizer/parser
-> CreateIndexStatement or DropIndexStatement
-> Executor
-> Catalog create_index(...) or drop_index(...)
-> catalog metadata update
-> physical index file create/remove
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

## Current CREATE/DROP INDEX behavior
The current executor supports first-pass SQL-driven secondary-index lifecycle
operations.

Supported forms:
```sql
CREATE INDEX users_age_idx ON users (age);
DROP INDEX users_age_idx;
```

Current behavior:
- delegates metadata changes to the catalog
- creates a physical B+ tree file for a new secondary index
- opens the table after `CREATE INDEX` and backfills existing rows into the new
  secondary index
- removes the physical B+ tree file when a secondary index is dropped
- rejects dropping primary indexes through `DROP INDEX`
- returns `affected_rows = 0` because this is DDL, not row-level DML

Current limitation:
- only non-primary integer columns can be indexed
- duplicate indexed integer values are supported through encoded
  `(indexed_value, primary_key)` B+ tree keys
- `SELECT` execution can use secondary integer indexes for equality, range,
  and `BETWEEN` predicates

## Current SELECT behavior
The current executor supports `SELECT` through either a full table scan or a
first index-backed candidate scan.

Supported forms:
- `SELECT * FROM table`
- `SELECT column_a, column_b FROM table`
- `WHERE` comparison predicates
- `WHERE BETWEEN`
- `WHERE AND`
- `WHERE OR`

The executor always evaluates `WHERE` before projection:
```text
scan or index-fetch full rows
-> filter using full table schema
-> project selected columns
```

This is important for queries such as:
```sql
SELECT id FROM users WHERE name = 'Alice';
```

The `name` column is still available during filtering even though the final
result only includes `id`.

When a supported index predicate exists, the executor first fetches candidate
rows through the table/index layer and then still applies the normal `WHERE`
filter. That keeps correctness simple for combined predicates such as:
```sql
SELECT * FROM users WHERE age = 30 AND id = 2;
```

Current index-scan support:
- primary-key equality uses direct primary-key lookup
- primary-key range predicates use primary-index range scan
- primary-key `BETWEEN` uses primary-index range scan
- secondary integer equality uses direct secondary-index lookup
- secondary integer range predicates use secondary-index range scan
- secondary integer `BETWEEN` uses secondary-index range scan
- `AND` expressions can use one indexable side and then filter the full
  expression afterward
- `OR` expressions can union both sides when both sides are indexable

Current index-scan limitation:
- `OR` expressions still fall back to the full scan path when either side is
  not indexable
- only integer predicates on primary keys or indexed secondary integer columns
  are index-backed
- there is no cost-based choice between multiple usable indexes yet

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

Filtering is still applied after row retrieval. Index scans only narrow the
candidate row set before the final `WHERE` evaluation.

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
- the B+ tree delete path removes leaf entries but does not rebalance yet

## Current limitations
- filtering is still in-memory after row retrieval
- `OR` index union only works when both branches are indexable
- there is no cost-based planner or physical operator tree yet
- secondary index scans currently only support integer columns that fit the
  current encoded key model
- auto-increment state is rebuilt from table rows on open instead of persisted
  as catalog/table metadata
- primary-key-changing `UPDATE` is not supported yet
- query result rows still use `table::Row`, so they carry `Row::key` even when
  the primary-key column is not projected

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

The current executor index test verifies:
- SQL-driven `CREATE INDEX`
- SQL-driven `DROP INDEX`
- metadata and physical file creation for secondary indexes
- backfilling existing rows into a secondary index during `CREATE INDEX`
- metadata and physical file removal for dropped secondary indexes
- duplicate-index creation failure
- string-column index rejection
- primary-key secondary-index rejection
- missing-table index creation failure
- primary-index drop rejection
- repeated secondary-index drop failure
- primary-key equality lookup through the index path
- primary-key range lookup through the index path
- secondary integer equality lookup through the index path
- secondary integer range lookup through the index path
- indexed `BETWEEN` lookup through primary and secondary indexes
- indexed predicate extraction from `AND`
- indexed `OR` union with duplicate-row removal
- `OR` fallback correctness through the normal filter path when needed

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
- refactor direct executor control flow into physical operators such as
  `SeqScan`, `IndexScan`, `Filter`, and `Projection`
- improve partial index usage for mixed indexable/non-indexable `OR`
- add better index selection when multiple predicates are indexable
- add SQL support for unique/non-unique index distinction
