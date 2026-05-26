#pragma once

#include <cstdint>
#include <string>

namespace db::table {

struct Row {
    std::uint32_t key;
    std::string value;
};

}  // namespace db::table