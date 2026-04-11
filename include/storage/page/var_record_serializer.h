#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "storage/page/var_record.h"

namespace db {

// Return the number of bytes needed to serialize the record.
std::uint16_t serialized_size(const VarRecord& record);

// std::vector<char> is used here because a serialized VarRecord has
// runtime-sized data: key bytes, length bytes, and a variable-length value.
// Serialize a VarRecord into a contiguous byte buffer.
std::vector<char> serialize_var_record(const VarRecord& record);

// Deserialize a VarRecord from raw bytes.
// Returns std::nullopt if the input is malformed.
std::optional<VarRecord> deserialize_var_record(const char* data, std::uint16_t length);

}   // namespace db
