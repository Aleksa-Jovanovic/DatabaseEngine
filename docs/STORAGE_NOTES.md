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
- read the last page and try to append through `SlottedPage`
- if the last page is full, allocate a new page and insert there
- fixed-size insert returns a `RowId` with `page_id` and `slot_index`
- variable-length insert returns a `RowId` with `page_id` and `slot_index`
- `get(row_id)` reads a fixed-size record directly by physical location
- `get_var_record(row_id)` reads a variable-length record directly by physical location
- `delete_record(row_id)` performs logical deletion through the slotted page
- `update_record(row_id, record)` updates a fixed-size record in place
- `update_var_record(row_id, record)` may return a new `RowId` if the updated record moves to a new slot
- `find(key)` scans all pages in order
- each page scan walks through slot entries and reads records through the slot directory
- data remains persisted in a page-based disk file through `DiskManager`

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

## Next likely storage upgrade
After the current slotted page abstraction:
- keep full scan lookup working through slots
- later consider reclaiming deleted space
- later decide how `HeapFile` should expose variable-length records
