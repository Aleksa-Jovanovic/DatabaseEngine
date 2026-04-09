#pragma once

#include <string>
#include <vector>

namespace db::catalog {

struct ColumnDefinition {
  std::string name;
  std::string type;
};

class Schema {
 public:
  Schema() = default;

 private:
  std::vector<ColumnDefinition> columns_;
};

}  // namespace db::catalog
