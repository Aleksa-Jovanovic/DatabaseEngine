# Index Notes

## Current indexing context
The project is moving into Phase 5, where the first B+ tree index will be
built on top of the existing heap-file storage layer.

The current storage model is:
- rows are stored in heap pages
- each stored row has a physical location described by `RowId`
- `RowId` contains:
  - `page_id`
  - `slot_index`

That means a `RowId` is a physical pointer to a row stored inside the heap
file.

## What the index should point to
The heap file stores the full row payload for the current storage model.

Right now that full payload is still a simplified record type such as:
- `Record`
- `VarRecord`

So although the project does not yet have full table schemas and arbitrary
columns, the heap already plays the role of storing the complete row payload
for the current record abstraction.

The first index should therefore not store full rows inside the index.
Instead, it should store:
- the search key
- a `RowId` pointing to the heap location of the row

## First index key/value model
For the first version of indexing, the natural mapping is:

`key -> RowId`

Where:
- `key` is the integer key field of the current record
- `RowId` points to the heap page and slot where the full record is stored

In the current simplified model, the `key` behaves like a primary-key-style
lookup key for the row.

Example:
- heap row stored at `RowId{7, 3}`
- row contents are the full record payload
- index leaf entry stores `42 -> RowId{7, 3}`

Lookup flow:
1. search the index for key `42`
2. get back `RowId{7, 3}`
3. go to heap page `7`, slot `3`
4. read the full stored row payload from the heap

## Current design direction for Phase 5
The first B+ tree version uses:
- `IndexKey` keys, currently backed by `uint64_t`
- `RowId` as leaf payload
- heap pages as the source of full row data

This keeps the architecture aligned with a normal heap-plus-index design:
- heap file stores full rows
- index stores search keys plus references to heap rows

Primary keys are encoded into `IndexKey` with `encode_primary_key(...)`.
Integer secondary indexes encode `(indexed_value, primary_key)` into one
`IndexKey`. This allows duplicate indexed values while keeping every concrete
B+ tree key unique.

## B+ tree pages vs storage pages
A B+ tree page is different from the slotted heap pages already implemented in
the storage layer.

The important distinction is:
- `Page` is still the generic fixed-size physical page abstraction
- heap pages interpret those bytes as slotted record storage
- B+ tree pages will interpret those bytes as index-node storage

So the underlying physical container stays the same, but the logical page
layout changes depending on page purpose.

That means the project should treat B+ tree nodes as their own page formats,
not as slotted heap pages reused for indexing.

## First B+ tree page definitions
For the first indexing version, the tree should likely use a simple fixed-entry
layout rather than a slotted-page design.

This keeps the implementation easier to reason about because:
- entries are fixed-size
- keys remain sorted in-place
- node capacity is straightforward to calculate
- split logic is easier to implement and debug

### Common B+ tree page header
Both leaf pages and internal pages should probably begin with a shared header.

A good first shared header would include:
- page type
- current key count
- max key capacity
- page id

Possible purpose of each field:
- `page_type` tells whether the page is a leaf or internal node
- `key_count` tells how many valid keys are currently stored
- `max_size` tells the node capacity for split checks
- `page_id` identifies the page itself

Depending on the later implementation, `parent_page_id` may also be useful,
but it does not need to be required in the very first version if the tree logic
is kept simple.

### Leaf page layout
A leaf page stores the actual searchable index entries.

For the first version, each leaf entry should be:
- `key`
- `RowId`

So the logical mapping is:
- `<key, RowId>`

Leaf pages should also likely include:
- a pointer to the next leaf page

That next-leaf pointer is useful because B+ trees typically keep leaf pages
linked left-to-right, which later makes range scans much easier.

First leaf page structure:
- common B+ tree header
- `next_leaf_page_id`
- sorted array of `<key, RowId>` entries

Leaf-page responsibilities:
- store sorted key-to-row-location mappings
- support point lookup by key
- support ordered insertion into the leaf
- split when full

### Internal page layout
An internal page does not store row locations directly.
Instead, it stores routing information that tells the search where to descend.

For the first version, internal pages should store:
- separator keys
- child page ids

The logical meaning is:
- keys divide the search space
- child page ids point to lower-level B+ tree pages

First internal page structure:
- common B+ tree header
- ordered separator keys
- child page ids

One common design is:
- `n` keys
- `n + 1` child pointers

That allows internal nodes to route searches between key ranges.

Internal-page responsibilities:
- decide which child page should be followed during search
- accept separator keys during split propagation
- split when full

## First Phase 5 implementation direction
For the first working B+ tree version, a good scope would be:
- fixed-width `IndexKey` keys
- `RowId` payloads in leaf pages
- fixed-size leaf entries
- fixed-size internal entries
- one common page header layout
- search first
- insert next
- split after insertion works

Deliberately out of scope for the first version:
- delete
- duplicate keys
- concurrency control
- WAL / recovery interaction
- variable-length index keys
- prefix compression or other space optimizations

## Agreed naming
The current agreed names for the first B+ tree implementation are:
- `BPlusTreePageType`
- `BPlusTreePageHeader`
- `BPlusTreeLeafEntry`
- `BPlusTreeInternalEntry`
- `BPlusTreeLeafPage`
- `BPlusTreeInternalPage`

This naming keeps a clear distinction between:
- page-level/node-level abstractions
- entry-level record layouts inside those pages

## Suggested file layout
A reasonable first file layout for the indexing subsystem would be:
- `include/index/bplustree_page.h`
- `include/index/bplustree_leaf.h`
- `include/index/bplustree_internal.h`
- `src/index/bplustree_leaf.cpp`
- `src/index/bplustree_internal.cpp`

Suggested responsibility split:
- `bplustree_page.h`
  - shared enums
  - shared header structs
  - shared low-level constants/helpers for B+ tree pages
- `bplustree_leaf.h/.cpp`
  - `BPlusTreeLeafEntry`
  - `BPlusTreeLeafPage`
  - leaf-page search/insert/split helpers
- `bplustree_internal.h/.cpp`
  - `BPlusTreeInternalEntry`
  - `BPlusTreeInternalPage`
  - internal-page routing/insert/split helpers

## First concrete page-layout definitions
For the first version, the common B+ tree page header should likely be:
- `page_type`
- `key_count`
- `max_size`
- `parent_page_id`

Leaf pages should then extend that with:
- `next_leaf_page_id`

Internal pages should use the common header and then store:
- ordered separator keys
- child page ids

### First concrete type direction
Common page-type enum:
- `BPlusTreePageType`
  - `Leaf`
  - `Internal`

Common page header:
- `BPlusTreePageHeader`
  - `BPlusTreePageType page_type`
  - `uint16_t key_count`
  - `uint16_t max_size`
  - `uint32_t parent_page_id`

Leaf entry:
- `BPlusTreeLeafEntry`
  - `IndexKey key`
  - `RowId row_id`

Internal entry:
- `BPlusTreeInternalEntry`
  - `IndexKey key`
  - `uint32_t child_page_id`

### Important internal-page note
For internal pages, one common design uses:
- `n` keys
- `n + 1` child pointers

That means the final internal-page implementation may need to represent child
pointers a little differently than a simple one-struct-per-entry model.

For now, `BPlusTreeInternalEntry` is still a useful conceptual type for
discussing separator-key-plus-child relationships, but the actual internal-page
storage layout should be chosen carefully once search and split logic are being
implemented.

## Current implemented page layouts
The current page-wrapper implementation uses these byte layouts.

Metadata page byte layout:
- page `0` is reserved for B+ tree metadata
- the metadata page currently stores:
  - a fixed magic value used to recognize initialized metadata
  - the persisted root page id

Leaf page byte layout:
- `| BPlusTreePageHeader | next_leaf_page_id | BPlusTreeLeafEntry[] |`

Internal page byte layout:
- `| BPlusTreePageHeader | leftmost_child_page_id | BPlusTreeInternalEntry[] |`

### Current internal-page routing convention
The current internal-page layout uses:
- one separate `leftmost_child_page_id`
- one `right_child_page_id` inside each `BPlusTreeInternalEntry`

That means an internal page with `n` keys is interpreted as:
- one leftmost child pointer
- `n` separator keys
- `n` right-side child pointers

This matches the logical B+ tree shape of:
- `n` keys
- `n + 1` child pointers

## Current implemented wrapper behavior
The current `BPlusTreeLeafPage` and `BPlusTreeInternalPage` wrappers can:
- initialize a fresh B+ tree page header
- initialize page-specific pointer metadata
- expose the common page header
- expose leaf/internal entries by index
- detect when a leaf page is full
- search for a key inside one leaf page
- insert one key and `RowId` into a leaf page in sorted order
- reject duplicate leaf keys in the current simplified design
- detect when an internal page is full
- route a search key to the correct child page inside one internal page
- insert one separator key and right-child pointer into an internal page
  relative to the child page that split
- reject duplicate internal separator keys in the current simplified design
- return `nullptr` for out-of-range `entry_at()` access

### Current internal-page insertion convention
The current internal-page wrapper now exposes:
- `insert_after_child(left_child_page_id, key, right_child_page_id)`

This means parent updates are expressed structurally as:
- a known left child already exists in the page
- a split promoted one separator key upward
- a new right child must be placed immediately after that left child

This is more precise than inserting only by key order because it preserves the
child relationship that caused the parent update.

## Current implemented tree behavior
The current `BPlusTree` implementation can:
- treat `INVALID_PAGE_ID` as an empty-tree root
- reserve page `0` as a metadata page for root persistence
- reload the persisted root page id when reopening the tree
- start search from a configured root page id
- inspect a page header to distinguish leaf vs internal nodes
- traverse internal pages using `find_child_page_id(key)`
- search a leaf page for a matching key
- insert into an empty tree by creating a single leaf root
- insert into a single leaf root while that root still has space
- split a full root leaf into left and right leaf pages
- create a new internal root after a root-leaf split
- promote the first key of the new right leaf into the new internal root
- descend through an existing internal root to find the correct target leaf
- insert into a non-root leaf while that leaf still has space
- split a non-root leaf and insert the promoted separator into the parent
  internal page when that parent still has free space
- update internal parents using a child-relative insert operation instead of
  key-only insertion
- split a full internal page and promote its middle separator upward
- keep the promoted internal separator only in the parent, not in either split
  child node
- create a new root when an internal split reaches the top of the tree
- persist the new root page id whenever the tree root changes
- reject duplicate keys through leaf-page insertion rules

## Current insert boundary
The current insert path now works for:
- empty-tree insert
- insert into a single leaf root
- first root-leaf split into a new internal root
- insert into an already multi-level tree
- split of a non-root leaf when the parent internal page still has room
- splitting a full internal page and propagating the split upward
- creating a taller tree after an internal-root split

Current simplifications:
- fixed-size `uint32_t` keys
- duplicate-key rejection
- page `0` is dedicated to index metadata in the current file format
- delete removes entries from leaf pages but does not rebalance the tree yet
- no concurrency control
- no WAL or recovery interaction
- no range-scan API yet, although leaf pages are already linked for it

## Current delete boundary
The current B+ tree has a first point-delete path for primary-key index usage.

Current behavior:
- find the target leaf for the key
- return the deleted `RowId` when the key exists
- remove the key from the leaf entry array
- mark the leaf page dirty when deletion succeeds

Current simplifications:
- no merge or redistribution after underflow
- no root shrinking after deletes
- no parent-separator cleanup after leaf contents change
- empty or sparse leaf pages can remain in the tree

This is enough for the current table/executor delete path because point lookup
for the deleted key stops finding that entry. Full B+ tree delete balancing is
left for a later index-maintenance pass.

## Current node sizing
The current tree no longer hard-codes a tiny test-only node capacity.

Leaf and internal node capacities are now computed from:
- `PAGE_SIZE`
- shared page-header bytes
- page-type-specific entry size

That means the current implementation still uses fixed-size entries, but the
maximum number of entries per node is derived from the physical page layout
instead of being manually set to a constant like `4`.

## Current test coverage
The current index page tests verify:
- leaf-page initialization
- internal-page initialization
- leaf/internal metadata getter and setter behavior
- entry read/write access through page wrappers
- leaf-page point lookup behavior
- leaf-page sorted insertion behavior
- duplicate-key rejection in one leaf page
- full-page rejection in one leaf page
- internal-page child routing behavior
- internal-page sorted insertion behavior
- duplicate-key rejection in one internal page
- full-page rejection in one internal page
- `entry_at()` returning `nullptr` when the index is out of range
- empty-tree B+ tree search
- single-leaf-root B+ tree search
- internal-root B+ tree search across two child leaves
- top-level B+ tree insert into an empty tree
- top-level B+ tree insert into a single leaf root
- duplicate rejection through the top-level insert path
- root-leaf split followed by successful search through the new internal root
- non-root leaf split absorbed by an internal root with free space
- recursive internal split propagation through a full internal root
- successful search after the tree grows taller through an internal-root split
- reopened-tree lookup using the persisted root page id
- leaf-level B+ tree delete
- delete of keys from a split tree without breaking neighboring lookups
- reopened-tree lookup after deleted keys were removed
