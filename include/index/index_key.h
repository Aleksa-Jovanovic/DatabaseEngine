#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace db::index {

using IndexKey = std::uint64_t;

inline IndexKey encode_primary_key(std::uint32_t primary_key) {
    return static_cast<IndexKey>(primary_key);
}

inline std::optional<IndexKey> encode_secondary_integer_key(
    std::int64_t indexed_value,
    std::uint32_t primary_key
) {
    if (
        indexed_value < 0 ||
        indexed_value > std::numeric_limits<std::uint32_t>::max()
    ) {
        return std::nullopt;
    }

    return (static_cast<IndexKey>(indexed_value) << 32) |
           static_cast<IndexKey>(primary_key);
}

}  // namespace db::index