#pragma once

#include <array>
#include <cstdint>
#include "common/constants.h"

namespace db {

struct Page {
  std::uint32_t page_id = 0;
  //char data[PAGE_SIZE];
  std::array<char, PAGE_SIZE> data{};
};

}