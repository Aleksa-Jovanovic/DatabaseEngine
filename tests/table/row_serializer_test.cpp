#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <variant>

#include "table/row.h"
#include "table/row_serializer.h"

int main() {
    {
        db::table::Row row{
            42,
            {
                std::int64_t{42},
                std::string{"Alice"},
                true,
                db::table::DateValue{"2026-06-26"}
            }
        };

        std::vector<char> bytes = db::table::RowSerializer::serialize(row);
        auto decoded = db::table::RowSerializer::deserialize(
            row.key,
            bytes.data(),
            bytes.size()
        );

        assert(decoded.has_value());
        assert(decoded->key == 42);
        assert(decoded->values.size() == 4);

        assert(std::get<std::int64_t>(decoded->values[0]) == 42);
        assert(std::get<std::string>(decoded->values[1]) == "Alice");
        assert(std::get<bool>(decoded->values[2]) == true);
        assert(std::get<db::table::DateValue>(decoded->values[3]).value == "2026-06-26");
    }

    {
        db::table::Row row{7, {}};

        std::vector<char> bytes = db::table::RowSerializer::serialize(row);
        auto decoded = db::table::RowSerializer::deserialize(
            row.key,
            bytes.data(),
            bytes.size()
        );

        assert(decoded.has_value());
        assert(decoded->key == 7);
        assert(decoded->values.empty());
    }

    {
        std::vector<char> invalid_bytes{1, 2, 3};
        auto decoded = db::table::RowSerializer::deserialize(
            99,
            invalid_bytes.data(),
            invalid_bytes.size()
        );

        assert(!decoded.has_value());
    }

    {
        auto decoded = db::table::RowSerializer::deserialize(99, nullptr, 0);
        assert(!decoded.has_value());
    }

    std::cout << "RowSerializer test passed.\n";
    return 0;
}
