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

The current `Table` API still supports direct constructor arguments, but it
also supports construction from `TableMetadata`.

This keeps the first Phase 6 version simple while starting to group table
configuration into one coherent object before the later catalog phase.

## Current row model
The current table abstraction still uses the existing fixed-size `Record` type:
- `key`
- `value`

That means the current "full row" returned by the table layer is still the
same simplified record used in earlier storage phases.

This is a temporary row model for the first table abstraction milestone.
Later phases may evolve this into a richer row representation with more fields
or serialized variable-length payloads.

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
1. insert the full record into the heap file
2. receive a `RowId` for the inserted row
3. insert `key -> RowId` into the primary B+ tree

The current implementation does not yet attempt rollback if the heap insert
succeeds but the index insert fails.

## Current update behavior
The current table layer has a first `update_by_key(...)` implementation.

Current behavior:
- resolve the key through the primary index
- get the corresponding `RowId`
- update the heap record in place

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
The current `Table` class already has a future-facing structure for additional
indexes keyed by column name.

However, only the primary index is currently active in behavior.
Secondary index management is still deferred.

## Current test coverage
The current table integration test verifies:
- table-level insert
- table-level lookup by primary key
- duplicate primary-key rejection through the table API
- same-key update behavior
- rejection of primary-key-changing updates
- persistence across reopen through heap and primary index files

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
- fixed-size `Record` row model
- `uint32_t` primary key
- no schema-aware row representation yet
- no secondary-index maintenance yet
- no delete path yet
- no rollback if heap insert succeeds and index insert fails
