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
- a list of secondary-index metadata entries

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
- `value` as `std::string`

The current row model is richer than the old fixed-size `Record`, but it is
still intentionally simple and only represents:
- one integer primary key
- one variable-length string value

The current table layer also has a `RowSerializer` with the byte layout:
- `[key][value_length][value_bytes]`

At the moment, the `Table` implementation still bridges through the existing
`VarRecord` heap-storage path because that storage shape already matches the
current `Row` model closely.

## Current lookup flow
The current key-based table lookup works like this:
1. search the primary B+ tree using the row key
2. get back a `RowId`
3. fetch the full stored record from the heap file using that `RowId`

This keeps the architecture aligned with a normal heap-plus-index design:
- heap file stores full row payload
- primary index stores key-to-row-location mapping

## Current insert flow
The current table insert works like this:
1. convert the logical `Row` into the current variable-length heap record shape
2. insert the full row into the heap file
3. receive a `RowId` for the inserted row
4. insert `key -> RowId` into the primary B+ tree

The current implementation does not yet attempt rollback if the heap insert
succeeds but the index insert fails.

## Current update behavior
The current table layer has a first `update_by_key(...)` implementation.

Current behavior:
- resolve the key through the primary index
- get the corresponding `RowId`
- update the heap record through the variable-length storage path
- repair the primary-index mapping if the updated row moves to a new `RowId`

Current limitation:
- only same-key updates are allowed
- changing the primary key is rejected

This keeps the first update path simple because changing the indexed key would
require coordinated index maintenance that is intentionally deferred.

## Current delete behavior
The current table API exposes a delete method shape, but full table-level
delete behavior is not implemented yet.

The main reason is that:
- heap-file delete exists
- B+ tree delete does not yet exist

So a correct table-level delete path should wait until index delete behavior is
available or until a deliberate temporary strategy is chosen.

## Current support for additional indexes
The current `Table` class now has a first explicit registration path for
secondary indexes.

Current `add_secondary_index(...)` behavior:
- rejects empty index names, column names, or file names
- rejects duplicate secondary-index names
- creates a `BPlusTree` for the registered secondary index
- stores runtime secondary-index information in the table object
- stores matching `IndexMetadata` entries in `TableMetadata`

Current metadata-load behavior:
- reconstructs runtime secondary-index objects from `TableMetadata`
- skips incomplete metadata entries with missing index names or file names
- ignores duplicate secondary-index names during runtime load so index state
  stays one-to-one by name

Current limitation:
- secondary-index registration does not yet backfill existing rows
- secondary indexes are not yet maintained during insert, update, or delete
- arbitrary column names can already appear in metadata, but that metadata is
  still future-facing until broader index-key support exists
- the current uniqueness flag is still constrained by the existing B+ tree,
  which currently rejects duplicate keys

## Current test coverage
The current table integration test verifies:
- table-level insert
- table-level lookup by primary key
- duplicate primary-key rejection through the table API
- same-key update behavior
- index repair when a variable-length update moves the row to a new physical location
- rejection of primary-key-changing updates
- persistence across reopen through heap and primary index files

The current row-serialization test verifies:
- serialize one `Row` into bytes
- deserialize bytes back into one `Row`
- empty-string round trip
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
- simple `Row { key, value }` row model
- `uint32_t` primary key
- current heap bridge still reuses `VarRecord`
- no schema-aware row representation yet
- no secondary-index backfill or maintenance yet
- no delete path yet
- no rollback if heap insert succeeds and index insert fails

## Phase 6 checkpoint
For the current project scope, Phase 6 now has:
- a working table-layer API
- metadata-based table construction
- row serialization support
- primary-index-backed insert and lookup
- same-key update behavior with index repair on row relocation
- first secondary-index metadata and registration scaffolding

That is a good stopping point before moving into catalog/schema persistence in
Phase 7.
