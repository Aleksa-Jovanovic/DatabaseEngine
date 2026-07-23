# Storage Notes

## Current storage design goal
Start with the smallest useful storage engine:
- fixed-size pages
- fixed-size records
- disk-backed page file
- sequential scan lookup

## First page format
Initial Phase 2 page layout:

| PageHeader | Record[] | Free space |

### PageHeader
Fields for first version:
- num_records

### Record
Fields for first version:
- key
- value

Both are fixed-size uint32_t fields for simplicity.

## First file layout
Database file is page-based:

[Page 0][Page 1][Page 2]...

Disk offset:
offset = page_id * PAGE_SIZE

## Current DiskManager behavior
- opens the database file in binary read/write mode
- creates the file first if it does not exist yet, then reopens it for reading and writing
- initializes `page_count` from `file_size / PAGE_SIZE`
- `allocate_page()` currently returns the next logical page id and increments `page_count`
- `write_page(page_id, data)` writes exactly one full page at offset `page_id * PAGE_SIZE`
- `write_page` only allows writes to already allocated pages
- `read_page(page_id, out_data)` reads exactly one full page from offset `page_id * PAGE_SIZE`
- if `read_page` is called for a page id beyond the current page count, it returns a zero-filled page buffer

## Current heap file behavior
- if the file has no pages yet, allocate the first page
- initialize a new page as a slotted page before first insert
- fetch the last page through `PageCacheManager` and try to append through `SlottedPage`
- if the last page is full, allocate a new page and insert there
- fixed-size insert returns a `RowId` with `page_id` and `slot_index`
- variable-length insert returns a `RowId` with `page_id` and `slot_index`
- `get(row_id)` fetches a cached page and reads a fixed-size record directly by physical location
- `get_var_record(row_id)` fetches a cached page and reads a variable-length record directly by physical location
- `delete_record(row_id)` performs logical deletion through the slotted page
- `update_record(row_id, record)` updates a fixed-size record in place
- `update_var_record(row_id, record)` may return a new `RowId` if the updated record moves to a new slot
- `find(key)` scans all pages in order
- each page scan walks through slot entries and reads records through the slot directory
- read and write operations now go through `PageCacheManager`
- write operations now mark cached pages dirty and rely on page-cache flushing
- dirty pages are currently written back on eviction, explicit flush, or page-cache destruction
- data remains persisted in a page-based disk file through `DiskManager` underneath the page cache layer

## Current simplifications
- no checksums
- no page type field yet

## Slotted page layout
Phase 3 now has a first slotted page implementation.

Current slotted page shape:

| SlottedPageHeader | SlotEntry[] | Free space | Record bytes |

Current layout behavior:
- slots grow from the front of the page toward higher offsets
- record bytes grow from the back of the page toward lower offsets
- free space stays between the slot directory and the record bytes

### SlottedPageHeader
Current fields:
- `slot_count`
- `free_space_start`
- `free_space_end`

### SlotEntry
Current fields:
- `offset`
- `length`

### Free space convention
- free space is the byte range `[free_space_start, free_space_end)`

### Current slotted page behavior
- `SlottedPage::initialize()` creates an empty slotted page
- `insert_record()` inserts one fixed-size `Record` if enough space remains
- `insert_var_record()` inserts one variable-length `VarRecord` if enough space remains
- `delete_record(slot_index)` performs logical deletion by invalidating the slot length
- `compact_page()` repacks live record bytes and rebuilds contiguous free space
- `record_at(slot_index)` reads a record through the slot directory
- `var_record_at(slot_index)` reads a variable-length record through the slot directory
- deleted slots are treated as unreadable
- `free_space()` returns the currently available free bytes in the page

### Current variable-length record format
The current variable-length record type is:
- `key` as `uint32_t`
- `value` as variable-length string data

Current serialized byte layout:
- `[key][value_length][value_bytes]`

Current serializer behavior:
- `serialized_size()` returns the total byte size needed for one serialized record
- `serialize_var_record()` produces a contiguous byte buffer
- `deserialize_var_record()` validates raw bytes and rebuilds a `VarRecord`

### Current deletion behavior
- deletion is logical, not physical
- deleting a slot does not remove the slot entry
- deleting a slot does not compact record bytes immediately
- deleting an already deleted slot returns false

### Current compaction behavior
- compaction preserves slot indexes
- compaction keeps the same number of slot entries
- compaction rewrites only live record bytes
- compaction updates slot offsets after records are moved
- compaction rebuilds one contiguous free-space region
- compaction is currently an explicit page-level operation

### Current scope of Phase 3
The slotted page implementation now exists both as a page-level abstraction and as the active page format used by `HeapFile`.

What is working now:
- page initialization
- slot directory management
- fixed-size record insertion into the page
- variable-length record insertion into the page
- slot-based record lookup inside the page
- variable-length record lookup inside the page
- isolated tests for basic insert/read behavior
- isolated test for full-page behavior
- isolated logical deletion test
- isolated compaction test
- isolated serializer test for `VarRecord`
- isolated slotted-page variable-length record test
- `HeapFile` insert now uses `SlottedPage`
- `HeapFile` fixed-size insert now returns a `RowId`
- `HeapFile` fixed-size get/update/delete work through `RowId`
- `HeapFile` variable-length insert/get/update work through `RowId`
- `HeapFile` full-scan lookup now reads records through slots
- disk-backed heap-file test passes with reopen/read verification
- disk-backed heap-file `RowId` test passes with reopen/read verification
- disk-backed heap-file variable-length test passes with reopen/read verification

## Current page cache layer
Phase 4 now has a first integrated `PageCacheManager` abstraction.

Current page cache goal:
- keep a fixed number of pages in memory
- reduce direct page reads and writes in higher storage layers
- track whether cached pages are dirty
- flush cached pages back to disk when needed

### Current page cache design
The current page cache uses:
- a fixed-size vector of `PageFrame` objects
- one `PageFrame` per cache slot
- a page table mapping `page_id -> frame_index`

### Current PageFrame fields
Each frame currently stores:
- `page`
- `is_dirty`
- `pin_count`
- `is_valid`
- `last_used_tick`

### Current page cache behavior
The current `PageCacheManager` can:
- fetch an existing page into memory with `fetch_page(page_id)`
- allocate a new logical page with `new_page()`
- mark a page dirty and release one pin with `unpin_page(page_id, is_dirty)`
- flush one cached page with `flush_page(page_id)`
- flush all valid cached pages with `flush_all_pages()`

Current `HeapFile` integration:
- `HeapFile` now uses `PageCacheManager` instead of talking directly to `DiskManager`
- heap-file reads fetch pages from the page cache and unpin them after access
- heap-file writes mark cached pages dirty and let the page cache handle write-back
- existing heap-file tests pass after the page-cache migration
- isolated `PageCacheManager` tests pass for persistence, pinned-frame behavior, and LRU eviction behavior

### Current replacement behavior
Current frame selection behavior:
- first use an invalid frame if one exists
- otherwise evict the least recently used frame whose `pin_count` is zero
- if all frames are pinned, page fetch or page allocation fails

### Current LRU behavior
The current page cache uses a simple timestamp-based LRU policy.

Current LRU tracking behavior:
- each frame stores `last_used_tick`
- `PageCacheManager` advances one global usage counter on page access
- cache hits update the touched frame's usage tick
- page loads into a frame update the new frame's usage tick
- newly allocated pages also get a fresh usage tick
- flush and unpin operations do not change recency

### Current test-only helpers
The current page cache exposes a small test-only helper:
- `is_page_cached(page_id)` is only compiled when `DB_TESTING` is enabled
- test targets define `DB_TESTING` through CMake
- the helper is used to verify LRU eviction decisions without making the normal runtime API larger

### Current page cache simplifications
- no concurrency control
- no latches
- no background flushing
- no explicit checkpointing policy yet
- no clock policy or more advanced replacement tuning yet

### Current per-file cache ownership
The current storage objects do not share one global buffer pool yet.

Current ownership model:
- each `HeapFile` owns its own `PageCacheManager`
- each primary `BPlusTree` owns its own `PageCacheManager`
- each secondary `BPlusTree` owns its own `PageCacheManager`
- each `PageCacheManager` owns its own file-specific `DiskManager`

This means a table with one heap file, one primary index file, and one
secondary index file currently has three separate page caches and three
separate disk managers.

Page ids are local to each physical file:
- `data/users/heap.db` can have page `1`
- `data/users/primary_index.db` can also have page `1`
- those are different pages because they belong to different files

B+ tree files reserve page `0` for B+ tree metadata such as the root page id.
Heap files currently use page `0` as a normal heap data page.

This per-file design is simple and works well for the current project stage,
but it is not the ideal architecture for a mature database engine. A more
database-style design would group page caching behind a shared buffer-pool or
storage-manager layer. In that design, heap files and index files would share
one cache, and pages would be identified by something like `(file_id, page_id)`
instead of only a local `page_id`.

Future benefits of a shared buffer pool:
- one global memory budget across heap and index pages
- one replacement policy across all table/index files
- fewer visibility issues when two objects access the same file
- a cleaner foundation for checkpointing, WAL, and recovery

### Current durability limitation
The current page cache uses write-back behavior, not crash-safe durability.

Current limitation:
- dirty pages can still be lost if power is lost before they are flushed
- flushing on eviction or destruction helps normal shutdown behavior but does not protect against crashes
- write-ahead logging and recovery are planned for a later phase

## Next likely storage upgrade
After the first page cache layer:
- add more tests around dirty-page write-back behavior
- later consider whether a clock-style replacement policy is worth adding
