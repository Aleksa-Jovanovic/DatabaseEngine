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
- no slot directory
- no free space pointer
- no checksums
- no page type field yet

## Next likely storage upgrade
After basic version works:
- add page type
- add free space tracking
- move toward slotted pages
