# Table Notes

## Current table abstraction goal
Phase 6 introduces a first table-level API on top of the existing heap-file
and indexing layers.

The purpose of this layer is to stop working directly with raw storage
subsystems in higher-level code and instead expose operations that behave more
like table access:
- insert a row
- look up a row by key
- later update or delete a row through the table abstraction

## Current table design
The current `Table` implementation combines:
- one `HeapFile`
- one primary `BPlusTree`

The current table layer now also has a lightweight `TableMetadata` struct.

Current metadata fields:
- table name
- heap file name
- primary index file name
- shared cache size
- column metadata entries
- a list of secondary-index metadata entries

Current column metadata fields:
- column name
- column type
- whether the column is the primary key
- whether the primary key should auto-increment when omitted by SQL execution

Current secondary-index metadata fields:
- index name
- column name
- index file name
- whether the index is primary
- whether the index is unique

The current `Table` API still supports direct constructor arguments, but it
also supports construction from `TableMetadata`.

This keeps the first Phase 6 version simple while starting to group table
configuration into one coherent object before the later catalog phase.

At the current end of Phase 6, this metadata is still in-memory only.
Persisting table definitions and index definitions is intentionally deferred to
the later catalog phase.

## Current row model
The current table abstraction now uses a table-layer `Row` type:
- `key`
- `values` as a vector of typed field values

Current supported field values:
- integer values
- string values
- boolean values
- date values represented as a small `DateValue` wrapper around a string

The row key is still stored separately from the serialized field payload:
- `Row::key` is the primary key used by the current B+ tree path
- `Row::values` is the logical table row payload

The current table layer also has a `RowSerializer` with the byte layout:
- `[field_count]`
- for each field:
  - `[type_tag]`
  - `[field_payload]`

The `Table` implementation stores typed rows through the existing
variable-length heap path:
- `VarRecord::key` stores the row primary key
- `VarRecord::value` stores the serialized `Row::values` byte payload

This lets the table layer support real multi-field rows without redesigning
the heap-file or slotted-page storage format yet.

## Current lookup flow
The current key-based table lookup works like this:
1. search the primary B+ tree using the row key
2. get back a `RowId`
3. fetch the full stored record from the heap file using that `RowId`

This keeps the architecture aligned with a normal heap-plus-index design:
- heap file stores full row payload
- primary index stores key-to-row-location mapping

## Current scan flow
The table layer now exposes a first full-scan path through `Table::scan()`.

The scan flow works like this:
1. ask the heap file to scan all variable-length records
2. deserialize each `VarRecord::value` back into row field values
3. reattach `VarRecord::key` as `Row::key`
4. return the visible logical rows to the caller

This is the foundation for the first SQL executor implementation. Early
`SELECT` execution can scan all rows and apply filtering/projection in memory
before index-scan optimization exists.

## Current insert flow
The current table insert works like this:
1. serialize `Row::values` into a byte payload with `RowSerializer`
2. store the payload in `VarRecord::value` and the primary key in
   `VarRecord::key`
3. insert the full `VarRecord` into the heap file
4. receive a `RowId` for the inserted row
5. insert `key -> RowId` into the primary B+ tree
6. insert maintained secondary-index entries for the row

If the heap insert succeeds but primary-index insertion fails, the table layer
deletes the just-written heap row. This keeps heap scans consistent with
primary-key lookups when duplicate keys or other index failures happen.

If secondary-index insertion fails after the heap and primary index succeed,
the table layer deletes the primary-index entry and heap row. It also rolls back
secondary-index entries already inserted during that same operation.

When column metadata is available, inserts validate the row before writing:
- the number of row values must match the number of columns
- each row value type must match the corresponding column type
- the primary-key column must be an integer
- the primary-key column value must match `Row::key`

Direct table construction without column metadata remains supported for older
tests and lower-level usage. In that mode, row validation is intentionally
skipped.

## Current primary-key allocation behavior
The table layer now keeps a live `next_primary_key_value_` counter for
auto-increment-style inserts.

`TableMetadata` can now mark a column as auto-increment, but the actual
decision to generate a missing primary-key value currently happens in the
execution layer before calling `Table::insert(...)`.

When a `Table` object opens, it scans existing rows once and initializes the
counter from the maximum existing primary key. Heap scan order is physical
storage order, not primary-key order, so the table must inspect all scanned
rows to find the maximum key.

Successful inserts advance the live counter when needed. This keeps manual
high-key inserts and later generated keys aligned.

Current limitation:
- the auto-increment counter is live runtime state only
- it is rebuilt from table rows when the table opens
- it is not persisted in catalog or table metadata yet

## Current update behavior
The current table layer has a first `update_by_key(...)` implementation.

Current behavior:
- resolve the key through the primary index
- get the corresponding `RowId`
- read the old row before overwriting it
- validate the updated row when column metadata is available
- update the heap record through the variable-length storage path
- repair the primary-index mapping if the updated row moves to a new `RowId`
- update maintained secondary-index entries

Current limitation:
- only same-key updates are allowed
- changing the primary key is rejected
- secondary-index update maintenance is not fully rollback-safe if a later
  secondary-index write fails after an earlier one has already changed

This keeps the first update path simple because changing the indexed key would
require coordinated index maintenance that is intentionally deferred.

## Current delete behavior
The current table API supports primary-key-based delete through
`Table::delete_by_key(...)`.

Current behavior:
- resolve the key through the primary B+ tree
- read the stored row so secondary-index keys can be reconstructed
- delete maintained secondary-index entries
- delete the key from the primary index
- delete the matching heap record
- return `false` when the key does not exist

Current limitation:
- there is no rollback if primary-index delete succeeds and heap delete fails
- there is no rollback if secondary-index delete succeeds and a later delete
  step fails

The current delete order prefers avoiding duplicate stale index entries, but a
crash or heap-delete failure between index delete and heap delete could leave a
row visible to heap scan but unreachable by primary-key lookup. WAL/recovery and
transactional rollback are intentionally deferred.

## Current support for additional indexes
The current `Table` class now has a first explicit registration path for
secondary indexes.

Current `add_secondary_index(...)` behavior:
- rejects empty index names, column names, or file names
- rejects duplicate secondary-index names
- creates a `BPlusTree` for the registered secondary index
- stores runtime secondary-index information in the table object
- stores matching `IndexMetadata` entries in `TableMetadata`

Current `backfill_secondary_index(...)` behavior:
- locates a registered secondary index by name
- scans existing heap records together with their `RowId`s
- encodes each row's secondary key for the indexed integer column
- inserts `encoded secondary key -> RowId` entries into the B+ tree
- rolls back entries inserted during the same backfill attempt if a later row
  fails

Current secondary-index key model:
- primary index keys are explicitly encoded with `encode_primary_key(...)`
- integer secondary index keys are encoded as `(indexed_value, primary_key)`
- duplicate indexed values are supported because the primary key makes each
  encoded secondary key unique

Current metadata-load behavior:
- reconstructs runtime secondary-index objects from `TableMetadata`
- skips incomplete metadata entries with missing index names or file names
- ignores duplicate secondary-index names during runtime load so index state
  stays one-to-one by name

Current limitation:
- arbitrary column names can already appear in metadata, but that metadata is
  still future-facing until broader index-key support exists
- SQL/catalog index management currently limits secondary indexes to
  non-primary integer columns
- secondary indexes are maintained on new inserts, same-primary-key updates,
  and deletes after the index exists
- SQL `CREATE INDEX` backfills rows that existed before the index was created
- secondary-index maintenance is not yet transactional or rollback-safe across
  every partial-failure scenario

## Current test coverage
The current table integration test verifies:
- table-level insert
- table-level lookup by primary key
- table-level full scan
- duplicate primary-key rejection through the table API
- rollback of the heap row when primary-index insertion fails
- same-key update behavior
- index repair when a variable-length update moves the row to a new physical location
- rejection of primary-key-changing updates
- primary-key delete behavior through index lookup plus heap delete
- schema-backed row validation for field count, field type, and primary-key value
- persistence across reopen through heap and primary index files
- secondary-index insert, update, and delete maintenance through direct B+
  tree verification of encoded secondary keys

The current row-serialization test verifies:
- serialize typed row fields into bytes
- deserialize bytes back into one `Row`
- integer, string, boolean, and date field round trips
- empty field-list round trip
- invalid or truncated input rejection

## Current cache-size behavior
The current table metadata includes one shared cache-size value.

That cache-size configuration is now passed consistently to:
- the heap file
- the primary B+ tree

The current design does not yet distinguish between heap-cache size and
index-cache size. If later workloads need different tuning, that can be split
into separate configuration fields.

## Current simplifications
- one heap file per table
- one active primary B+ tree index
- typed `Row { key, values }` row model
- `uint32_t` primary key
- current heap bridge still reuses `VarRecord`
- row values are schema-validated only when `TableMetadata` contains columns
- primary key is still stored separately from the logical field vector
- auto-increment metadata is persisted through catalog definitions
- auto-increment counter state is rebuilt from table rows on open instead of
  persisted as a stored sequence value
- secondary-index maintenance is not transactional or rollback-safe yet
- delete does not rebalance the B+ tree yet
- delete is not transactional or rollback-safe yet

## Phase 6 checkpoint
For the current project scope, Phase 6 now has:
- a working table-layer API
- metadata-based table construction
- row serialization support
- primary-index-backed insert and lookup
- full table scan support for the first execution layer
- same-key update behavior with index repair on row relocation
- primary-key delete behavior
- first secondary-index metadata and registration scaffolding
- SQL/catalog secondary-index lifecycle exists, but table write maintenance for
  those indexes is still future work

That is a good stopping point before moving into catalog/schema persistence in
Phase 7.
