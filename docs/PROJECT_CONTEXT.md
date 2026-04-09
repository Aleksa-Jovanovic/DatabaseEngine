# Project Context

## Project
Build a small educational database engine in C++ from low-level storage up to higher-level database concepts.

## Main goals
- Learn how database storage works on disk
- Implement fixed-size pages and page-based file I/O
- Understand record layout, heap storage, and indexing
- Later build toward B+ tree indexes, catalog, simple SQL parsing, and query execution
- Use the project as a learning project, portfolio project, and possible thesis foundation

## Current knowledge baseline
The project is guided by concepts from:
- Designing Data-Intensive Applications
- Database Internals

Current studied topics include:
- page layout
- log-structured storage
- page-oriented storage
- record storage basics
- B-tree / B+ tree basics
- indexing fundamentals

## Current implementation stage
Phase 1 / early Phase 2:
- define basic storage structures
- define page format
- implement disk manager
- implement simple heap file
- support insert and full scan lookup

## Non-goals for the first version
- no SQL yet
- no query planner
- no transactions
- no WAL / crash recovery
- no concurrency control
- no full catalog implementation
- no B+ tree yet

## Development approach
Build bottom-up:
1. raw storage and disk I/O
2. pages
3. records inside pages
4. heap file
5. better page layout
6. buffer pool
7. B+ tree
8. table abstraction
9. SQL and execution layer

## Style principles
- keep first version simple and correct
- prefer explicit byte layout over abstraction too early
- use plain structs for on-disk layout
- use classes for behavior and storage logic
- test each layer before moving upward