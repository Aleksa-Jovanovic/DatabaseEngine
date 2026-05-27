# Catalog Notes

## Current catalog goal
Phase 7 introduces the first catalog layer for the database.

The purpose of this layer is to track database-wide metadata instead of
constructing tables and indexes purely from handwritten runtime configuration.

At a high level:
- tables store user data
- schemas describe table structure
- the catalog tracks those definitions for the whole database

## Current schema model
The current schema layer lives in `catalog::Schema`.

Current schema pieces:
- `ColumnType`
  - `Integer`
  - `String`
  - `Boolean`
  - `Date`
- `ColumnDefinition`
  - column name
  - column type
  - `is_primary_key`
- `Schema`
  - owns a list of column definitions
  - supports both full construction from a vector and incremental
    `add_column(...)`

This gives the project a first structured description of table columns instead
of relying on ad hoc names alone.

## Current catalog metadata model
The catalog metadata layer is intentionally separate from table runtime
metadata.

Current catalog metadata types:
- `IndexDefinition`
  - index name
  - table name
  - column name
  - file name
  - `is_primary`
  - `is_unique`
- `TableDefinition`
  - table name
  - schema
  - heap file name
  - `indexes`
- `CatalogMetadata`
  - collection of table definitions

At the catalog level, all table indexes now live in one `indexes` list.
Primary indexes are identified by `is_primary = true`.

## Current Catalog behavior
The current `Catalog` now supports both:
- pure in-memory construction
- file-backed construction from a catalog metadata file

Current public behavior:
- create a table definition
- check whether a table exists
- find a table definition by name
- open a runtime `Table` from catalog metadata
- expose the underlying `CatalogMetadata`

Current validation:
- duplicate table names are rejected
- empty table names are rejected

Current persistence behavior:
- a file-backed catalog loads its metadata blob on construction
- missing catalog file is treated as an empty catalog
- `create_table(...)` persists immediately for file-backed catalogs
- if a save fails during `create_table(...)`, the in-memory append is rolled back

## Current Table reconstruction flow
The first important catalog-to-runtime bridge is now implemented.

Current flow:
1. look up a `TableDefinition` by name
2. scan the catalog-level `indexes`
3. extract exactly one primary index definition
4. convert non-primary index definitions into runtime `table::IndexMetadata`
5. build `table::TableMetadata`
6. construct a runtime `table::Table`

This is the current role of `Catalog::open_table(...)`.

## Current serialization format
Catalog metadata is now serialized through `CatalogSerializer`.

Current serialized structure:
- catalog magic number
- catalog format version
- number of tables
- for each table:
  - table name
  - heap file name
  - schema columns
  - index definitions

Current deserialization checks:
- matching magic number
- matching format version
- valid column-type values
- strict full-buffer consumption with no trailing garbage

## Current testing
The current catalog test verifies:
- table creation
- duplicate table rejection
- empty-name rejection
- schema fields are stored correctly
- unified index definitions include both primary and non-primary indexes
- `build_table_metadata(...)` succeeds for valid catalog state
- missing-table lookup and metadata-build failure behavior
- missing-primary-index rejection during runtime metadata reconstruction
- `open_table(...)` succeeds for known tables and fails for unknown ones
- file-backed catalog metadata persists across reopen
- reopened catalog can still reconstruct and open a known table

## Current limitations
- there is no table drop or alter behavior yet
- there is no validation yet for:
  - duplicate index names within one table
  - invalid schemas
  - multiple primary-key columns
  - multiple primary indexes
- catalog currently bridges to the existing table runtime shape rather than to
  a fully catalog-driven runtime engine
- persistence format versioning is still minimal and only handles one version
- there is no explicit crash-safety or write-ahead logging for catalog writes

## Current phase boundary
This is the first structural slice of Phase 7.

The catalog now exists as:
- a schema model
- a metadata model
- an in-memory and file-backed registry
- a bridge into runtime table construction
- a first catalog serializer and reopen path
