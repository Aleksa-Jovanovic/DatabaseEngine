#pragma once

#include <string>
#include <utility>
#include <vector>

namespace db::catalog {

enum class ColumnType {
    Integer,
    String,
    Boolean,
    Date
};

struct ColumnDefinition {
    std::string name;
    ColumnType type;
    bool is_primary_key = false;
};

class Schema {
public:
    Schema() = default;

    explicit Schema(std::vector<ColumnDefinition> columns)
        : columns_(std::move(columns)) {}

    inline void add_column(const ColumnDefinition& column) {
        columns_.push_back(column);
    }

    inline const std::vector<ColumnDefinition>& columns() const {
        return columns_;
    }

private:
    std::vector<ColumnDefinition> columns_;
};

}  // namespace db::catalog