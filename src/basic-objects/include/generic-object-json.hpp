#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>

#include "cct_cctype.hpp"
#include "cct_json-serialization.hpp"

namespace cct::details {

template <auto Opts, class B, class IX>
constexpr bool JsonWithQuotes(B &&b, IX &&ix) {
  if constexpr (Opts.prettify) {
    auto begIt = std::reverse_iterator(b.data() + ix);
    auto endIt = std::reverse_iterator(b.data());
    auto foundIfNotIt = std::find_if_not(begIt, endIt, [](char ch) { return isspace(ch); });

    return foundIfNotIt != endIt && *foundIfNotIt == ':';
  } else {
    return ix != 0 && b[ix - 1] == ':';
  }
}

}  // namespace cct::details