#pragma once

#include <string>

namespace db::sql {

class Parser {
 public:
  void Parse(const std::string& sql) const;
};

}  // namespace db::sql
