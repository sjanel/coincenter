#pragma once

#include <cstdint>

#include "cct_cctype.hpp"
#include "cct_json-serialization.hpp"

namespace cct::details {

template <auto Opts, class B, class IX>
constexpr bool JsonWithQuotes(B &&b, IX &&ix) {
  if (ix == 0) {
    return false;
  }

  // This is a hack waiting for resolution of this issue: https://github.com/stephenberry/glaze/issues/1477
  const char *pFirstChar = b.data();
  const char *pChar = pFirstChar + ix - 1;

  if constexpr (Opts.prettify) {
    while (isspace(*pChar) && --pChar != pFirstChar);
  }

  if (*pChar == ':') {
    return true;
  }

  return *pChar != '"';
}

template <auto Opts, class B, class IX>
constexpr void ToStrLikeJson(auto &&value, B &&b, IX &&ix) {
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