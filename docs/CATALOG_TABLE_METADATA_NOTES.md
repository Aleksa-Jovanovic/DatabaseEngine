# Catalog, TableMetadata, and Table Notes

## Purpose
This note explains why the project has both catalog-level metadata and
table-level runtime metadata.

## Catalog
`Catalog` is the database-wide metadata registry.

It answers questions such as:
- what tables exist
- what columns each table has
- which column is the primary key
- which heap and index files belong to a table
- which indexes are defined for a table

`CatalogMetadata` is the persistent/global source of truth for database object
definitions.

The catalog does not insert rows, update records, or scan heap pages directly.

## TableMetadata
`TableMetadata` is the table-local runtime configuration object.

It is derived from catalog table definitions when a table is opened.

It contains only the information one `Table` instance needs:
- table name
- heap file name
- primary index file name
- cache size
- column metadata
- secondary-index metadata

This keeps the table layer independent from catalog-specific types such as
`TableDefinition`, `Schema`, and `IndexDefinition`.

## Table
`Table` is the runtime access object for one table.

It owns or manages:
- `HeapFile`
- primary `BPlusTree`
- secondary index handles
- table-local metadata

The physical access fields perform storage and index operations:
- `heap_file_`
- `primary_index_`
- `secondary_indexes_`

`metadata_` is still useful after construction because it describes what rows
mean logically:
- column names
- column types
- primary-key column
- configured index definitions

## Relationship
The flow is:

```text
CatalogMetadata
-> TableDefinition
-> TableMetadata
-> Table
-> HeapFile / BPlusTree
```

The catalog stores the database-wide definition.

The table metadata is the prepared runtime recipe for one table.

The table uses that recipe to open storage/index objects and to understand the
logical shape of rows.

## Future Execution Flow
The SQL executor should talk to both catalog and table:

```text
SQL AST
-> Executor
-> Catalog: find schema and open table
-> Table: insert/get/update/delete rows
-> Storage/index layers: persist and locate bytes
```

Example insert flow:

```text
INSERT statement
-> executor checks table schema in Catalog
-> executor builds Row values in schema column order
-> executor calls Table::insert(row)
-> Table serializes Row::values into VarRecord::value
-> HeapFile stores the row payload
-> BPlusTree stores primary-key -> RowId
```

## Key Distinction
`CatalogMetadata` is the source of truth for what exists.

`TableMetadata` is the runtime description of one table.

`Table` is the object that actually performs table-level operations.
