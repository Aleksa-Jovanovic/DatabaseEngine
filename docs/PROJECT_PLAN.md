# Project Plan

## Phase 1 - Storage foundation
- define constants like PAGE_SIZE
- define Page struct
- define PageHeader
- define Record struct
- implement DiskManager
- allocate pages
- read/write pages to disk

Goal:
Persist bytes correctly using fixed-size pages.

## Phase 2 - Simple heap file
- basic page layout with header + fixed-size records
- append records to page
- allocate new page when full
- scan pages to find a record

Goal:
Store and retrieve records without indexing.

## Phase 3 - Better page layout
- free space tracking
- slotted page design
- variable-length records
- deletion handling
- update handling

Goal:
Make page layout closer to real database pages.

Current status:
Phase 3 is functionally complete in the current implementation.
The project now has slotted pages, logical deletion, variable-length records,
update handling, compaction, and heap-file integration built on top of the
new page layout.

## Phase 4 - Buffer pool
- cache pages in memory
- track dirty pages
- flush pages to disk
- later add replacement policy

Goal:
Separate disk I/O from memory page access.

Current status:
Phase 4 is functionally complete in the current implementation.
The project now has an integrated `PageCacheManager`, dirty-page tracking,
write-back flushing, heap-file integration, and a simple LRU replacement
policy for unpinned cached pages.

## Phase 5 - Indexing
- B+ tree page layout
- internal and leaf pages
- search
- insert
- split pages
- later improve indexing to support additional index types, including
  duplicate-key secondary indexes and non-integer keys such as strings

Goal:
Enable fast key-based lookup.

Current status:
Phase 5 is in progress.
The project now has B+ tree page-layout definitions, leaf/internal page
wrappers, local leaf-page search and sorted insert behavior, local
internal-page routing and child-relative insert behavior, and top-level B+
tree search plus insert support for an empty tree, a single leaf root, the
first root-leaf split into a new internal root, non-root leaf splits,
internal split propagation, and creation of a taller tree after an
internal-root split. The B+ tree also now has leaf-entry delete support and a
`range_scan(...)` API that walks linked leaves for ordered key ranges, all with
dedicated tests or notes coverage.

## Phase 6 - Table abstraction
- table metadata
- heap storage + indexes
- row identifier
- insert/update/delete/get row

Goal:
Represent user data as tables rather than raw pages.

Current status:
Phase 6 is functionally complete for the current scoped milestone.
The project now has a first `Table` abstraction that combines one heap file
with one primary B+ tree index, a lightweight `TableMetadata` structure for
table configuration, column metadata, and index metadata tracking, a typed
multi-field table-layer `Row` model plus row-serialization support,
schema-backed row validation for insert and update, support for table-level
insert, key-based lookup, and same-key update behavior with index repair when
variable-length row updates move the stored row, full table scan support for
the first execution layer, and a first secondary-index registration path plus
metadata-driven runtime reconstruction, all with initial integration-test
coverage.

## Phase 7 - Catalog
- schemas
- table definitions
- index definitions
- metadata persistence
- later improve catalog validation to return structured errors (for example
  `CatalogValidationError`) instead of only boolean success/failure

Goal:
Track database objects and schema.

Current status:
Phase 7 is in progress.
The project now has a first catalog layer with schema metadata,
catalog-level table and index definitions, a `Catalog` registry that can
create and look up table definitions, a serializer-backed metadata file
format for catalog persistence, reopen support for file-backed catalogs, and
a bridge from catalog definitions into runtime `TableMetadata` plus `Table`
reconstruction through `open_table(...)`, along with initial table-definition
validation rules around primary keys, indexes, and schema consistency.

## Phase 8 - SQL layer
- tokenizer
- parser
- AST
- basic commands like CREATE, INSERT, SELECT

Goal:
Allow simple database interaction through SQL-like syntax.

Current status:
Phase 8 is functionally complete for the current parser/tokenizer/AST
milestone.
The project now has a first SQL tokenizer that classifies keywords,
identifiers, type names, numeric literals, string literals, punctuation, and
an explicit end-of-input token, along with typed AST and parser paths for
`CREATE TABLE`, `INSERT`, `SELECT`, `DELETE`, and `UPDATE` statements.
`INSERT` parsing supports both schema-order values and optional named column
lists, `SELECT` parsing supports `*` and explicit projected columns, and
`WHERE` parsing now builds expression trees for comparison predicates,
`BETWEEN`, `AND`, and `OR` with SQL-style precedence.

## Phase 9 - Execution
- sequential scan
- index scan
- filtering
- projection
- simple execution pipeline

Goal:
Execute parsed queries.

Current status:
Phase 9 is in progress.
The project now has a first execution layer with an `Executor` and
`ExecutionResult`, wired through catalog table lookup, table scans, and first
index-backed candidate scans. The current executor supports `SELECT`, explicit
projection, comparison filtering, `BETWEEN`, and logical `AND`/`OR` filtering
with parser-provided SQL precedence. It also supports SQL-driven
`CREATE TABLE` with `PRIMARY KEY` and `AUTOINCREMENT` column metadata,
SQL-driven `DROP TABLE` with catalog and physical-file cleanup, first-pass
SQL-driven `CREATE INDEX` and `DROP INDEX` for integer secondary indexes,
`INSERT` execution with schema-order values, named columns, simple defaults for
omitted non-primary columns, and live auto-increment for omitted
auto-increment primary keys. Execution now supports `UPDATE` for
non-primary-key assignments with affected-row counts and `DELETE` with
affected-row counts. The table layer maintains integer secondary indexes on
insert, same-primary-key update, and delete using encoded
`(indexed_value, primary_key)` B+ tree keys, and SQL `CREATE INDEX` backfills
existing rows into the new secondary index. `SELECT` can now use primary and
secondary integer indexes for equality and range predicates, including one
indexable predicate inside an `AND` expression, indexed `BETWEEN`, and `OR`
union when both branches are indexable, while still applying the full `WHERE`
filter afterward. Cost-based index selection, mixed indexable/non-indexable
`OR` optimization, non-integer secondary indexes, and transactional
index-maintenance rollback are still pending. The current executor is still a
direct control-flow implementation rather than a physical operator tree; a
future refactor can introduce explicit operators such as `SeqScan`,
`IndexScan`, `Filter`, and `Projection` on top of the same table/index APIs.

## Phase 10 - Transactions and recovery
- transactions
- locking/latching basics
- WAL
- recovery

Goal:
Support correctness and durability.

Current status:
Deferred for now. The project already has a functional single-threaded storage,
catalog, SQL parsing, and execution path. Transactions, WAL, and crash recovery
remain important database-engine topics, but they are intentionally paused while
the project moves into a presentation/demo layer.

## Phase 11 - Local web frontend and demo
- local web interface
- SQL query input
- query result display
- schema/catalog display
- table row browser
- simple execution feedback

Goal:
Make the database engine easier to present and interact with through a small
local frontend.

Planned scope:
- provide a text area for writing SQL queries
- execute queries against the current C++ database engine
- show query results in a table-like result panel
- show catalog metadata such as tables, columns, primary keys, and indexes
- show stored table rows for the selected table
- optionally show whether a query used a sequential scan or index-backed path

Notes:
This phase is presentation-focused rather than a storage-engine correctness
phase. A simple local web UI is preferred over only a CLI because it makes the
engine's behavior easier to demonstrate during a thesis/project presentation.
