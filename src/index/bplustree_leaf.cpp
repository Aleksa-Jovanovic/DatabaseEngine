#include "index/bplustree_leaf.h"

namespace db::index {

BPlusTreeLeafPage::BPlusTreeLeafPage(Page& page) : page_(page) {}

void BPlusTreeLeafPage::initialize(std::uint16_t max_size) {
    BPlusTreePageHeader* header = fetch_header();
    
    header->page_type = BPlusTreePageType::Leaf;
    header->key_count = 0;
    header->max_size = max_size;
    header->parent_page_id = INVALID_PAGE_ID;

    set_next_leaf_page_id(INVALID_PAGE_ID);
}

BPlusTreePageHeader* BPlusTreeLeafPage::fetch_header() {
    return reinterpret_cast<BPlusTreePageHeader*>(data());
}

const BPlusTreePageHeader* BPlusTreeLeafPage::fetch_header() const {
    return reinterpret_cast<const BPlusTreePageHeader*>(data());
}

std::uint32_t BPlusTreeLeafPage::next_leaf_page_id() const {
    return *next_leaf_page_id_ptr();
}

void BPlusTreeLeafPage::set_next_leaf_page_id(std::uint32_t page_id) {
    *next_leaf_page_id_ptr() = page_id;
}

std::uint16_t BPlusTreeLeafPage::key_count() const {
    const BPlusTreePageHeader* header = fetch_header();

    return header->key_count;
}

std::uint16_t BPlusTreeLeafPage::max_size() const {
    const BPlusTreePageHeader* header = fetch_header();

    return header->max_size;
}

bool BPlusTreeLeafPage::is_full() const {
    return key_count() >= max_size();
}

const BPlusTreeLeafEntry* BPlusTreeLeafPage::find_entry(std::uint32_t key) const {
    const std::int32_t index = find_key_index(key);
    if (index < 0) {
        return nullptr;
    }

    return entry_at(static_cast<std::uint16_t>(index));
}

bool BPlusTreeLeafPage::insert_entry(std::uint32_t key, const RowId& row_id) {
    if (is_full()) {
        return false;
    }

    // Do not add duplicate keys
    if (find_key_index(key) >= 0) {
        return false;
    }

    const std::uint16_t insert_index = find_insert_position(key);
    BPlusTreeLeafEntry* leaf_entries = entries();

    for (std::uint16_t i = key_count(); i > insert_index; --i) {
        leaf_entries[i] = leaf_entries[i - 1];
    }

    leaf_entries[insert_index].key = key;
    leaf_entries[insert_index].row_id = row_id;

    ++fetch_header()->key_count;
    return true;
}

bool BPlusTreeLeafPage::delete_entry(std::uint32_t key) {
    const std::int32_t key_index = find_key_index(key);
    if (key_index < 0) {
        return false;
    }

    BPlusTreeLeafEntry* leaf_entries = entries();
    const std::uint16_t delete_index = static_cast<std::uint16_t>(key_index);

    for (std::uint16_t i = delete_index; i + 1 < key_count(); ++i) {
        leaf_entries[i] = leaf_entries[i + 1];
    }

    --fetch_header()->key_count;
    return true;
}

BPlusTreeLeafEntry* BPlusTreeLeafPage::entry_at(std::uint16_t index) {
    if (index >= key_count()) {
        return nullptr;
    }

    return &entries()[index];
}

const BPlusTreeLeafEntry* BPlusTreeLeafPage::entry_at(std::uint16_t index) const {
    if (index >= key_count()) {
        return nullptr;
    }

    return &entries()[index];
}

char* BPlusTreeLeafPage::data() {
    return page_.data.data();
}

const char* BPlusTreeLeafPage::data() const {
    return page_.data.data();
}

std::uint32_t* BPlusTreeLeafPage::next_leaf_page_id_ptr() {
    return reinterpret_cast<std::uint32_t*>(
        data() + sizeof(BPlusTreePageHeader)
    );
}

const std::uint32_t* BPlusTreeLeafPage::next_leaf_page_id_ptr() const {
    return reinterpret_cast<const std::uint32_t*>(
        data() + sizeof(BPlusTreePageHeader)
    );
}

BPlusTreeLeafEntry* BPlusTreeLeafPage::entries() {
    return reinterpret_cast<BPlusTreeLeafEntry*>(
        data() + sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t)
    );
}

const BPlusTreeLeafEntry* BPlusTreeLeafPage::entries() const {
    return reinterpret_cast<const BPlusTreeLeafEntry*>(
        data() + sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t)
    );
}

std::int32_t BPlusTreeLeafPage::find_key_index(std::uint32_t key) const {
    for (std::uint16_t i = 0; i < key_count(); ++i) {
        const BPlusTreeLeafEntry* entry = entry_at(i);
        if (entry == nullptr) {
            return -1;
        }

        if (entry->key == key) {
            return static_cast<std::int32_t>(i);
        }

        if (entry->key > key) {
            break;
        }
    }

    return -1;
}

std::uint16_t BPlusTreeLeafPage::find_insert_position(std::uint32_t key) const {
    std::uint16_t i = 0;

    while (i < key_count()) {
        const BPlusTreeLeafEntry* entry = entry_at(i);
        if (entry == nullptr || entry->key >= key) {
            break;
        }

        ++i;
    }

    return i;
}

}  // namespace db::index
