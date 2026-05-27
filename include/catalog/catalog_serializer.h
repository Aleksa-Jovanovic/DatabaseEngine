#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "catalog/catalog_metadata.h"

namespace db::catalog {

class CatalogSerializer {
public:
    // Serializes the full database-wide catalog metadata blob.
    static std::vector<char> serialize(const CatalogMetadata& metadata);
    // Rebuilds catalog metadata from one serialized blob and rejects malformed
    // or version-mismatched input.
    static std::optional<CatalogMetadata> deserialize(const char* data, std::size_t size);
};

}  // namespace db::catalog
