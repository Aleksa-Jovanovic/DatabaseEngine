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

}  // namespace db::index
