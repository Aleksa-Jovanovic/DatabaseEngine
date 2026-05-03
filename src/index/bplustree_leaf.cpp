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

}  // namespace db::index
