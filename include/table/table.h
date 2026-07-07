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

    std::uint32_t allocate_primary_key();

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
    std::uint32_t next_primary_key_value_ = 1;

    void load_secondary_indexes_from_metadata();
    bool validate_row(const Row& row) const;
    void initialize_next_primary_key_value();
    void advance_next_primary_key_value(std::uint32_t key);

    std::optional<index::IndexKey> encode_secondary_key_for_row(
        const Row& row,
        const std::string& column_name
    ) const;

    bool insert_secondary_index_entries(
        const Row& row,
        const RowId& row_id
    );
    bool delete_secondary_index_entries(const Row& row);
    bool update_secondary_index_entries(
        const Row& old_row, 
        const Row& new_row, 
        const RowId& new_row_id
    );
};

}  // namespace db::table
