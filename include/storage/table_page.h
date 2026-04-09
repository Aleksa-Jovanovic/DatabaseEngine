#pragma once

#include "storage/page.h"

namespace db::storage {

class TablePage : public Page {
 public:
  TablePage() = default;
};

}  // namespace db::storage
