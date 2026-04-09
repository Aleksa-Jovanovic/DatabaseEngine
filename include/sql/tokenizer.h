#pragma once

#include <string>
#include <vector>

namespace db::sql {

class Tokenizer {
 public:
  std::vector<std::string> Tokenize(const std::string& sql) const;
};

}  // namespace db::sql
