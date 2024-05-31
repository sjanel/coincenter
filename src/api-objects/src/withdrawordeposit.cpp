#include "withdrawordeposit.hpp"

#include <string_view>

#include "cct_string.hpp"
#include "timestring.hpp"
#include "unreachable.hpp"

namespace cct {
std::string_view WithdrawOrDeposit::statusStr() const {
  switch (_status) {
    case Status::kInitial:
      return "initial";
    case Status::kProcessing:
      return "processing";
    case Status::kFailureOrRejected:
      return "failed";
    case Status::kSuccess:
      return "success";
    default:
      unreachable();
  }
}

string WithdrawOrDeposit::timeStr() const { return TimeToString(_time); }
}  // namespace cct