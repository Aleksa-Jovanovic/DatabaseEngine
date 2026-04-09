#pragma once

#include <string>
#include <vector>

namespace db::sql {

struct AstNode {
  std::string kind;
  std::vector<AstNode> children;
};

}  // namespace db::sql
