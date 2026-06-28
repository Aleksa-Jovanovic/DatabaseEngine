#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "index/bplustree.h"
#include "storage/heap/heap_file.h"
#include "table/row.h"
#include "table/table_metadata.h"

namespace db::table {

class Table {
public:
    Table(
        const std::string& table_name,
        const std::string& heap_file_name,
        const std::string& index_file_name,
        std::size_t cache_size = 8
    );

    explicit Table(const TableMetadata& metadata);

    std::optional<RowId> insert(const Row& row);
    std::optional<Row> get_by_key(std::uint32_t key);
    std::vector<Row> scan();
    bool update_by_key(std::uint32_t key, const Row& updated_row);
    bool delete_by_key(std::uint32_t key);

    bool add_secondary_index(
        const std::string& index_name,
        const std::string& column_name,
        const std::string& index_file_name
    );

private:
    struct SecondaryIndexInfo {
        std::string index_name;
        std::string column_name;
        std::string file_name;
        bool is_unique;
        std::unique_ptr<index::BPlusTree> tree;
    };

    TableMetadata metadata_;
    HeapFile heap_file_;
    index::BPlusTree primary_index_;
    std::unordered_map<std::string, SecondaryIndexInfo> secondary_indexes_;

    void load_secondary_indexes_from_metadata();
    bool validate_row(const Row& row) const;
};

}  // namespace db::table
