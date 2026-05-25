#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "index/bplustree.h"
#include "storage/heap/heap_file.h"

namespace db::table {

class Table {
public:
    Table(
        const std::string& table_name,
        const std::string& heap_file_name,
        const std::string& index_file_name,
        std::size_t cache_size = 8
    );

    std::optional<RowId> insert(const Record& record);
    std::optional<Record> get_by_key(std::uint32_t key);
    bool update_by_key(std::uint32_t key, const Record& updated_record);
    bool delete_by_key(std::uint32_t key);

private:
    struct SecondaryIndexInfo {
        std::string column_name;
        std::unique_ptr<index::BPlusTree> tree;
    };

    std::string table_name_;
    HeapFile heap_file_;
    index::BPlusTree primary_index_;
    std::unordered_map<std::string, SecondaryIndexInfo> secondary_indexes_;
};

}  // namespace db::table