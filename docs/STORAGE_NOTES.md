# Storage Notes

## Current storage design goal
Start with the smallest useful storage engine:
- fixed-size pages
- fixed-size records
- disk-backed page file
- sequential scan lookup

## First page format
Initial page layout:

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

## Insert strategy - first version
- if file has no pages, allocate first page
- read last page
- if there is space, append record
- if full, allocate new page and append there

## Read / find strategy - first version
- scan all pages in order
- scan all records inside each page
- return first matching key

## Current simplifications
- no deletes
- no updates
- no variable-size records
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
- `record_at(slot_index)` reads a record through the slot directory
- `free_space()` returns the currently available free bytes in the page

### Current scope of Phase 3
The slotted page implementation currently exists as a page-level abstraction.

What is working now:
- page initialization
- slot directory management
- fixed-size record insertion into the page
- slot-based record lookup inside the page
- isolated tests for basic insert/read behavior
- isolated test for full-page behavior

What is not migrated yet:
- `HeapFile` still uses the earlier contiguous record layout

## Next likely storage upgrade
After the current slotted page abstraction:
- migrate `HeapFile` to use `SlottedPage`
- keep full scan lookup working through slots
- later add deletion handling
- later move toward variable-length records
