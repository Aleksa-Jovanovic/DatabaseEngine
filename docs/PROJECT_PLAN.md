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
internal-root split, all with dedicated tests or notes coverage.

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
table configuration plus index metadata tracking, a simple table-layer `Row`
type plus row-serialization support, support for table-level insert,
key-based lookup, and same-key update behavior with index repair when
variable-length row updates move the stored row, and a first secondary-index
registration path plus metadata-driven runtime reconstruction, all with
initial integration-test coverage.

## Phase 7 - Catalog
- schemas
- table definitions
- index definitions
- metadata persistence

Goal:
Track database objects and schema.

Current status:
Phase 7 is in progress.
The project now has a first in-memory catalog layer with schema metadata,
catalog-level table and index definitions, a `Catalog` registry that can
create and look up table definitions, and a bridge from catalog definitions
into runtime `TableMetadata` plus `Table` reconstruction through
`open_table(...)`.

## Phase 8 - SQL layer
- tokenizer
- parser
- AST
- basic commands like CREATE, INSERT, SELECT

Goal:
Allow simple database interaction through SQL-like syntax.

## Phase 9 - Execution
- sequential scan
- index scan
- filtering
- projection
- simple execution pipeline

Goal:
Execute parsed queries.

## Phase 10 - Transactions and recovery
- transactions
- locking/latching basics
- WAL
- recovery

Goal:
Support correctness and durability.
