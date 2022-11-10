#include "deposit.hpp"

#include "timestring.hpp"

namespace cct {
string Deposit::receivedTimeStr() const { return ToString(_receivedTime); }
}  // namespace cct