#include "withdrawordeposit.hpp"

#include <string_view>

#include "cct_string.hpp"
#include "timestring.hpp"
#include "unreachable.hpp"

namespace cct {
std::string_view WithdrawOrDeposit::statusStr() const {
  switch (_status) {
    case Status::initial:
      return "initial";
    case Status::processing:
      return "processing";
    case Status::failed:
      return "failed";
    case Status::success:
      return "success";
    default:
      unreachable();
  }
}

string WithdrawOrDeposit::timeStr() const { return TimeToString(_time); }
}  // namespace cct