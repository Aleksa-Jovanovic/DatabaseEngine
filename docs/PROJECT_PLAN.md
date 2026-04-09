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

## Phase 4 - Buffer pool
- cache pages in memory
- track dirty pages
- flush pages to disk
- later add replacement policy

Goal:
Separate disk I/O from memory page access.

## Phase 5 - Indexing
- B+ tree page layout
- internal and leaf pages
- search
- insert
- split pages

Goal:
Enable fast key-based lookup.

## Phase 6 - Table abstraction
- table metadata
- heap storage + indexes
- row identifier
- insert/update/delete/get row

Goal:
Represent user data as tables rather than raw pages.

## Phase 7 - Catalog
- schemas
- table definitions
- index definitions
- metadata persistence

Goal:
Track database objects and schema.

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