#pragma once

#include <cstdint>

namespace db {

// For data that represents one stored row.
struct Record {
    std::uint32_t key;
    std::uint32_t value;
};

}