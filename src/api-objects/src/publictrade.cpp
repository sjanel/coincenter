#include "publictrade.hpp"

#include "cct_string.hpp"
#include "timestring.hpp"

namespace cct {
string PublicTrade::timeStr() const { return ToString(_time); }
}  // namespace cct