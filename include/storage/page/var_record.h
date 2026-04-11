#pragma once

#include <cstdint>
#include <string>

namespace db {

// For data that represents one stored row.
struct VarRecord {
    std::uint32_t key;
    std::string value;
};

} // namespace db