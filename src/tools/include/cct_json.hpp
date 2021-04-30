
#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "cct_vector.hpp"

namespace cct {

using json = nlohmann::basic_json<std::map, cct::vector, std::string, bool, std::int64_t, std::uint64_t, double,
                                  amc::allocator, nlohmann::adl_serializer, cct::vector<std::uint8_t>>;

}  // namespace cct
