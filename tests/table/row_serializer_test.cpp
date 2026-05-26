#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "table/row.h"
#include "table/row_serializer.h"

int main() {
    {
        db::table::Row row{42, "hello"};

        std::vector<char> bytes = db::table::RowSerializer::serialize(row);
        auto decoded = db::table::RowSerializer::deserialize(bytes.data(), bytes.size());

        assert(decoded.has_value());
        assert(decoded->key == 42);
        assert(decoded->value == "hello");
    }

    {
        db::table::Row row{7, ""};

        std::vector<char> bytes = db::table::RowSerializer::serialize(row);
        auto decoded = db::table::RowSerializer::deserialize(bytes.data(), bytes.size());

        assert(decoded.has_value());
        assert(decoded->key == 7);
        assert(decoded->value.empty());
    }

    {
        std::vector<char> invalid_bytes{1, 2, 3};
        auto decoded = db::table::RowSerializer::deserialize(
            invalid_bytes.data(),
            invalid_bytes.size()
        );

        assert(!decoded.has_value());
    }

    {
        auto decoded = db::table::RowSerializer::deserialize(nullptr, 0);
        assert(!decoded.has_value());
    }

    std::cout << "RowSerializer test passed.\n";
    return 0;
}