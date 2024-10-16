#pragma once

#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>

#include "cct_allocator.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct::json {

using container = ::nlohmann::basic_json<::std::map, ::cct::vector, ::cct::string, bool, int64_t, uint64_t, double,
                                         ::cct::allocator, ::nlohmann::adl_serializer, ::cct::SmallVector<uint8_t, 8>>;

}  // namespace cct::json
