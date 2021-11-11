
#pragma once

#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>

#include "cct_allocator.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct {

using json = nlohmann::basic_json<std::map, vector, string, bool, int64_t, uint64_t, double, allocator,
                                  nlohmann::adl_serializer, SmallVector<uint8_t, 8>>;

}  // namespace cct
