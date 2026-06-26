#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace db::table {

struct DateValue {
    std::string value;
};

using FieldValue = std::variant<
    std::int64_t,
    std::string,
    bool,
    DateValue
>;

struct Row {
    std::uint32_t key;
    std::vector<FieldValue> values;
};

}  // namespace db::table