#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

#include "index/bplustree.h"
#include "index/index_key.h"
#include "table/table.h"

namespace {

db::table::Row make_user_row(
    std::uint32_t key,
    std::int64_t age,
    const std::string& name
) {
    return db::table::Row{
        key,
        {
            std::int64_t{key},
            age,
            name
        }
    };
}

db::index::IndexKey require_secondary_key(
    std::int64_t indexed_value,
    std::uint32_t primary_key
) {
    const auto encoded_key =
        db::index::encode_secondary_integer_key(indexed_value, primary_key);
    assert(encoded_key.has_value());
    return encoded_key.value();
}

}  // namespace

int main() {
    const std::string heap_file_name = "table_secondary_index_heap.db";
    const std::string primary_index_file_name = "table_secondary_index_primary.db";
    const std::string age_index_file_name = "table_secondary_index_age.db";

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);
    std::filesystem::remove(age_index_file_name);

    db::table::TableMetadata metadata{
        "users",
        heap_file_name,
        primary_index_file_name,
        8,
        {
            {"id", db::table::ColumnType::Integer, true},
            {"age", db::table::ColumnType::Integer, false},
            {"name", db::table::ColumnType::String, false}
        },
        {
            {
                "users_age_idx",
                "age",
                age_index_file_name,
                false,
                false
            }
        }
    };

    db::RowId row_id_1{};
    db::RowId row_id_2{};
    db::RowId row_id_3{};

    {
        db::table::Table table(metadata);

        const auto row_1 = table.insert(make_user_row(1, 30, "Alice"));
        const auto row_2 = table.insert(make_user_row(2, 30, "Bob"));
        const auto row_3 = table.insert(make_user_row(3, 40, "Carol"));

        assert(row_1.has_value());
        assert(row_2.has_value());
        assert(row_3.has_value());

        row_id_1 = row_1.value();
        row_id_2 = row_2.value();
        row_id_3 = row_3.value();
    }

    {
        db::index::BPlusTree age_index(age_index_file_name, 8);

        const auto age_30_user_1 =
            age_index.search(require_secondary_key(30, 1));
        const auto age_30_user_2 =
            age_index.search(require_secondary_key(30, 2));
        const auto age_40_user_3 =
            age_index.search(require_secondary_key(40, 3));

        assert(age_30_user_1.has_value());
        assert(age_30_user_1->page_id == row_id_1.page_id);
        assert(age_30_user_1->slot_index == row_id_1.slot_index);

        assert(age_30_user_2.has_value());
        assert(age_30_user_2->page_id == row_id_2.page_id);
        assert(age_30_user_2->slot_index == row_id_2.slot_index);

        assert(age_40_user_3.has_value());
        assert(age_40_user_3->page_id == row_id_3.page_id);
        assert(age_40_user_3->slot_index == row_id_3.slot_index);
    }

    {
        db::table::Table table(metadata);

        assert(table.update_by_key(2, make_user_row(2, 35, "Bobby")));
    }

    {
        db::index::BPlusTree age_index(age_index_file_name, 8);

        assert(!age_index.search(require_secondary_key(30, 2)).has_value());
        assert(age_index.search(require_secondary_key(35, 2)).has_value());
        assert(age_index.search(require_secondary_key(30, 1)).has_value());
        assert(age_index.search(require_secondary_key(40, 3)).has_value());
    }

    {
        db::table::Table table(metadata);

        assert(table.delete_by_key(1));
    }

    {
        db::index::BPlusTree age_index(age_index_file_name, 8);

        assert(!age_index.search(require_secondary_key(30, 1)).has_value());
        assert(age_index.search(require_secondary_key(35, 2)).has_value());
        assert(age_index.search(require_secondary_key(40, 3)).has_value());
    }

    std::filesystem::remove(heap_file_name);
    std::filesystem::remove(primary_index_file_name);
    std::filesystem::remove(age_index_file_name);

    std::cout << "Table secondary index test passed.\n";
    return 0;
}
