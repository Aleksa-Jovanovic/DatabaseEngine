#include "index/bplustree_internal.h"

namespace db::index {

BPlusTreeInternalPage::BPlusTreeInternalPage(Page& page) : page_(page) {}

void BPlusTreeInternalPage::initialize(std::uint16_t max_size) {
    BPlusTreePageHeader* header = fetch_header();

    header->page_type = BPlusTreePageType::Internal;
    header->key_count = 0;
    header->max_size = max_size;
    header->parent_page_id = INVALID_PAGE_ID;

    set_leftmost_child_page_id(INVALID_PAGE_ID);
}

BPlusTreePageHeader* BPlusTreeInternalPage::fetch_header() {
    return reinterpret_cast<BPlusTreePageHeader*>(data());
}

const BPlusTreePageHeader* BPlusTreeInternalPage::fetch_header() const {
    return reinterpret_cast<const BPlusTreePageHeader*>(data());
}

std::uint32_t BPlusTreeInternalPage::leftmost_child_page_id() const {
    return *leftmost_child_page_id_ptr();
}

void BPlusTreeInternalPage::set_leftmost_child_page_id(std::uint32_t page_id) {
    *leftmost_child_page_id_ptr() = page_id;
}

std::uint16_t BPlusTreeInternalPage::key_count() const {
    const BPlusTreePageHeader* header = fetch_header();

    return header->key_count;
}

std::uint16_t BPlusTreeInternalPage::max_size() const {
    const BPlusTreePageHeader* header = fetch_header();

    return header->max_size;
}

bool BPlusTreeInternalPage::is_full() const {
    return key_count() >= max_size();
}

BPlusTreeInternalEntry* BPlusTreeInternalPage::entry_at(std::uint16_t index) {
    if (index >= key_count()) {
        return nullptr;
    }

    return &entries()[index];
}

const BPlusTreeInternalEntry* BPlusTreeInternalPage::entry_at(std::uint16_t index) const {
    if (index >= key_count()) {
        return nullptr;
    }

    return &entries()[index];
}

std::uint32_t BPlusTreeInternalPage::find_child_page_id(std::uint32_t key) const {
    std::uint32_t candidate = leftmost_child_page_id();

    for (std::uint16_t i = 0; i < key_count(); ++i) {
        const BPlusTreeInternalEntry* entry = entry_at(i);
        if (entry == nullptr) {
            return candidate;
        }

        if (key < entry->key) {
            return candidate;
        }

        candidate = entry->right_child_page_id;
    }

    return candidate;
}

bool BPlusTreeInternalPage::insert_entry(std::uint32_t key, std::uint32_t right_child_page_id) {
    if (is_full()) {
        return false;
    }

    // Do not add duplicate keys.
    if (find_key_index(key) >= 0) {
        return false;
    }

    const std::uint16_t insert_index = find_insert_position(key);
    BPlusTreeInternalEntry* internal_entries = entries();

    for (std::uint16_t i = key_count(); i > insert_index; --i) {
        internal_entries[i] = internal_entries[i - 1];
    }

    internal_entries[insert_index].key = key;
    internal_entries[insert_index].right_child_page_id = right_child_page_id;

    ++fetch_header()->key_count;
    return true;
}


char* BPlusTreeInternalPage::data() {
    return page_.data.data();
}

const char* BPlusTreeInternalPage::data() const {
    return page_.data.data();
}

std::uint32_t* BPlusTreeInternalPage::leftmost_child_page_id_ptr() {
    return reinterpret_cast<std::uint32_t*>(
        data() + sizeof(BPlusTreePageHeader)
    );
}

const std::uint32_t* BPlusTreeInternalPage::leftmost_child_page_id_ptr() const {
    return reinterpret_cast<const std::uint32_t*>(
        data() + sizeof(BPlusTreePageHeader)
    );
}

BPlusTreeInternalEntry* BPlusTreeInternalPage::entries() {
    return reinterpret_cast<BPlusTreeInternalEntry*>(
        data() + sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t)
    );
}

const BPlusTreeInternalEntry* BPlusTreeInternalPage::entries() const {
    return reinterpret_cast<const BPlusTreeInternalEntry*>(
        data() + sizeof(BPlusTreePageHeader) + sizeof(std::uint32_t)
    );
}

std::int32_t BPlusTreeInternalPage::find_key_index(std::uint32_t key) const {
    for (std::uint16_t i = 0; i < key_count(); ++i) {
        const BPlusTreeInternalEntry* entry = entry_at(i);
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

std::uint16_t BPlusTreeInternalPage::find_insert_position(std::uint32_t key) const {
    std::uint16_t i = 0;

    while (i < key_count()) {
        const BPlusTreeInternalEntry* entry = entry_at(i);
        if (entry == nullptr || entry->key >= key) {
            break;
        }

        ++i;
    }

    return i;
}

}  // namespace db::index
