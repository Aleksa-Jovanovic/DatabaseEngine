#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "storage/page/var_record.h"
#include "storage/page/var_record_serializer.h"

// Basic serializer test for variable-length records:
// serialize a VarRecord, deserialize it back, and verify malformed input
// is rejected.
int main() {
    db::VarRecord record{42, "Variable_length storage test"};

    const std::uint16_t expected_size = 
        static_cast<std::uint16_t>(sizeof(std::uint32_t) + sizeof(std::uint16_t) + record.value.size());
    
    assert(db::serialized_size(record) == expected_size);

    std::vector<char> bytes = db::serialize_var_record(record);
    assert(bytes.size() == expected_size);

    auto deserialized = db::deserialize_var_record(bytes.data(), static_cast<std::uint16_t>(bytes.size()));
    assert(deserialized.has_value());
    assert(deserialized->key == 42);
    assert(deserialized->value == "Variable_length storage test");

    // Malformed input: too short to contain the fixed header.
    auto too_short = db::deserialize_var_record(bytes.data(), 2);
    assert(!too_short.has_value());

    // Malformed input: mismatched total length.
    auto wrong_length = db::deserialize_var_record(bytes.data(),
        static_cast<std::uint16_t>(bytes.size() - 1));
    assert(!wrong_length.has_value());

    std::cout << "VarRecord serializer test passed.\n";
    return 0;
}