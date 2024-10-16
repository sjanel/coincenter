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

template <auto Opts, class B, class IX>
constexpr void ToJson(auto &&value, B &&b, IX &&ix) {
  auto valueLen = value.strLen();
  bool withQuotes = JsonWithQuotes<Opts>(b, ix);

  int64_t additionalSize = (withQuotes ? 2L : 0L) + static_cast<int64_t>(ix) + static_cast<int64_t>(valueLen) -
                           static_cast<int64_t>(b.size());
  if (additionalSize > 0) {
    b.append(additionalSize, ' ');
  }

  if (withQuotes) {
    b[ix++] = '"';
  }
  value.appendTo(b.data() + ix);
  ix += valueLen;
  if (withQuotes) {
    b[ix++] = '"';
  }
}

}  // namespace cct::details