#pragma once

#include <cstdint>

namespace db {

//For structs that describe how bytes inside a page are organized
struct PageHeader {
    std::uint32_t num_records;
};

}